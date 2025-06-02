#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "bt_master.h"

#define FACE_SENSOR_PIN 0
#define BUTTON_PIN 1
#define LED_PIN 2
#define BUZZER_PIN 3
#define TRIG_PIN 4
#define ECHO_PIN 5
#define SPI_CH 0
#define ADC_CS 29
#define SPI_SPEED 500000
#define TEMP_SENSOR_CH 0
#define SOUND_SENSOR_CH 1
#define HUMID_SENSOR_CH 2
#define TEMP_THRESHOLD 30
#define HUMID_THRESHOLD 70
#define NOISE_THRESHOLD 70

void bluetooth_notify_user(const char* message);

bool user_authenticated = false;
bool focus_mode = false;
int focus_time_min = 0;
int noise_time_min = 0;
const int stable_threshold = 5;
const int distance_min = 30;
const int distance_max = 100;
int temp_sum = 0;
int humid_sum = 0;
int noise_sum = 0;
int env_sample_count = 0;
int bt_client = -1;

int readADC(int adcChannel) {
    unsigned char buf[3];
    int value;
    buf[0] = 0x06 | ((adcChannel & 0x04) >> 2);
    buf[1] = ((adcChannel & 0x03) << 6);
    buf[2] = 0x00;
    digitalWrite(ADC_CS, 0);
    wiringPiSPIDataRW(SPI_CH, buf, 3);
    digitalWrite(ADC_CS, 1);
    buf[1] = 0x0F & buf[1];
    value = (buf[1] << 8) | buf[2];
    return value;
}

void setup() {
    if (wiringPiSetup() == -1 || wiringPiSPISetup(SPI_CH, SPI_SPEED) < 0) {
        printf("❌ SPI 초기화 실패\n");
        exit(1);
    }
    pinMode(FACE_SENSOR_PIN, INPUT);
    pinMode(BUTTON_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(ADC_CS, OUTPUT);
    digitalWrite(TRIG_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(ADC_CS, 1);
}

void turn_on_led() {
    digitalWrite(LED_PIN, HIGH);
}

void turn_off_led() {
    digitalWrite(LED_PIN, LOW);
}

bool face_recognition() {
    bluetooth_notify_user("😀 얼굴 인식 시뮬레이션: 인증 성공");
    return true;
}

bool button_pressed() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        delay(100);
        while (digitalRead(BUTTON_PIN) == LOW);
        return true;
    }
    return false;
}

void activate_buzzer() {
    digitalWrite(BUZZER_PIN, HIGH);
    bluetooth_notify_user("🔔 부저 작동 (10초 연속 불집중)");
    delay(2000);
    digitalWrite(BUZZER_PIN, LOW);
}

int read_distance() {
    long start_time, end_time;
    float distance;
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    int timeout = 1000000;
    while (digitalRead(ECHO_PIN) == LOW && timeout-- > 0) delayMicroseconds(1);
    if (timeout <= 0) return -1;

    start_time = micros();
    timeout = 1000000;
    while (digitalRead(ECHO_PIN) == HIGH && timeout-- > 0) delayMicroseconds(1);
    if (timeout <= 0) return -1;

    end_time = micros();
    long travel_time = end_time - start_time;
    distance = travel_time / 58.0;
    return (distance < 2 || distance > 400) ? -1 : (int)distance;
}

bool is_user_focused() {
    static int prev_distance = -1;
    int current_distance = read_distance();
    if (current_distance < distance_min || current_distance > distance_max) return false;
    if (prev_distance == -1) {
        prev_distance = current_distance;
        return true;
    }
    int diff = abs(current_distance - prev_distance);
    prev_distance = current_distance;
    return (diff <= stable_threshold);
}

void read_environment(int* temperature, int* humidity, int* noise) {
    *temperature = readADC(TEMP_SENSOR_CH) * 100 / 1023;
    *humidity = readADC(HUMID_SENSOR_CH) * 100 / 1023;
    *noise = readADC(SOUND_SENSOR_CH) / 10;
}

void record_entry_time() {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char msg[128];
    snprintf(msg, sizeof(msg),
        "🕒 입장 시각: %04d-%02d-%02d %02d:%02d:%02d",
        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec);
    bluetooth_notify_user(msg);
}

void generate_focus_report() {
    double focus_score = focus_time_min - (noise_time_min * 0.5);
    double avg_temp = env_sample_count ? (double)temp_sum / env_sample_count : 0;
    double avg_humid = env_sample_count ? (double)humid_sum / env_sample_count : 0;
    double avg_noise = env_sample_count ? (double)noise_sum / env_sample_count : 0;

    char summary[256];
    snprintf(summary, sizeof(summary),
             "📈 집중 점수: %.1f점, 집중: %d분, 소음: %d분",
             focus_score, focus_time_min, noise_time_min);
    bluetooth_notify_user(summary);

    FILE* fp = fopen("focus_report.txt", "a");
    if (fp != NULL) {
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] 집중 리포트\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec);
        fprintf(fp, "  집중 시간: %d 분\n", focus_time_min);
        fprintf(fp, "  소음 시간: %d 분\n", noise_time_min);
        fprintf(fp, "  집중 점수: %.1f 점\n", focus_score);
        fprintf(fp, "  평균 온도: %.1f°C\n", avg_temp);
        fprintf(fp, "  평균 습도: %.1f%%\n", avg_humid);
        fprintf(fp, "  평균 소음: %.1f\n", avg_noise);
        fprintf(fp, "-----------------------------------\n\n");
        fclose(fp);
    }
}

void init_bluetooth_server_once() {
    if (bt_client == -1) {
        bt_client = init_server();
        printf("✅ 블루투스 서버 연결 완료 (client socket: %d)\n", bt_client);
    }
}

void bluetooth_notify_user(const char* message) {
    if (bt_client != -1) {
        write_server(bt_client, (char *)message);
    }
}

void notify_admin(const char* message) {
    char command[512];
    snprintf(command, sizeof(command),
             "curl -G --data-urlencode \"msg=%s\" http://192.168.0.120/alert_logger.php",
             message);
    system(command);
}

int main() {
    setup();
    init_bluetooth_server_once();

    while (1) {
        user_authenticated = face_recognition();
        if (!user_authenticated) continue;

        record_entry_time();
        bluetooth_notify_user("✅ 얼굴 인증 성공\n📌 버튼을 눌러 집중 모드를 시작하세요.");

        while (!button_pressed());

        focus_mode = true;
        focus_time_min = 0;
        noise_time_min = 0;
        turn_on_led();
        bluetooth_notify_user("🎯 집중 모드 시작");

        int unfocused_count = 0;

        while (focus_mode) {
            int focus_count = 0;
            int noise_count = 0;

            for (int i = 0; i < 60; i++) {
                if (is_user_focused()) {
                    focus_count++;
                    unfocused_count = 0;
                } else {
                    unfocused_count++;
                    if (unfocused_count == 10) {
                        activate_buzzer();
                        notify_admin("⚠ 집중 상태 10회 연속 이탈 감지됨");
                    }
                }

                int temp, humid, noise;
                read_environment(&temp, &humid, &noise);
                temp_sum += temp;
                humid_sum += humid;
                noise_sum += noise;
                env_sample_count++;

                bool env_issue = false;
                char admin_msg[256] = "환경 경고 - ";
                char user_msg[256] = "⚠ 환경 상태 이상: ";

                if (temp > TEMP_THRESHOLD) {
                    strcat(admin_msg, "온도 초과 ");
                    strcat(user_msg, "온도↑ ");
                    env_issue = true;
                }
                if (humid > HUMID_THRESHOLD) {
                    strcat(admin_msg, "습도 초과 ");
                    strcat(user_msg, "습도↑ ");
                    env_issue = true;
                }
                if (noise > NOISE_THRESHOLD) {
                    strcat(admin_msg, "소음 초과 ");
                    strcat(user_msg, "소음↑ ");
                    env_issue = true;
                    noise_count++;
                }

                if (env_issue) {
                    notify_admin(admin_msg);
                    bluetooth_notify_user(user_msg);
                }

                sleep(1);
                if (button_pressed()) {
                    focus_mode = false;
                    break;
                }
            }

            if (focus_mode) {
                if (focus_count >= 45) focus_time_min++;
                if (noise_count >= 20) noise_time_min++;
            }
        }

        turn_off_led();
        bluetooth_notify_user("🛑 집중 모드 종료됨\n📄 리포트 생성 중...");
        generate_focus_report();

        temp_sum = 0;
        humid_sum = 0;
        noise_sum = 0;
        env_sample_count = 0;
        bluetooth_notify_user("📥 다음 사용자 대기 중...\n\n");
    }

    return 0;
}
