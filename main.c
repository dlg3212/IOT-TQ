#include <stdio.h>
#include <wiringPi.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#define FACE_SENSOR_PIN 0      // 예시 핀 번호
#define BUTTON_PIN 1
#define LED_PIN 2
#define BUZZER_PIN 3
#define TRIG_PIN 4
#define ECHO_PIN 5
#define TEMP_SENSOR_CH 0
#define SOUND_SENSOR_CH 1
#define HUMID_SENSOR_CH 2

bool user_authenticated = false;
bool focus_mode = false;
int focus_time_min = 0;
int noise_time_min = 0;
const int stable_threshold = 5;   // 변화량이 5cm 이하 → 안정적
// distance 단위: cm
const int distance_min = 30;      // 너무 가까우면 센서 오류
const int distance_max = 100;     // 너무 멀면 자리 이탈로 간주
int temp_sum = 0;
int humid_sum = 0;
int noise_sum = 0;
int env_sample_count = 0;

int readADC(int adcChannel) {  // 외부 ADC 값 읽는 함수
    unsigned char buf[3];
    int value;

    buf[0] = 0x06 | ((adcChannel & 0x04) >> 2);
    buf[1] = ((adcChannel & 0x03) << 6);
    buf[2] = 0x00;

    digitalWrite(ADC_CS, 0);              // Start communication
    wiringPiSPIDataRW(SPI_CH, buf, 3);
    digitalWrite(ADC_CS, 1);              // End communication

    buf[1] = 0x0F & buf[1];
    value = (buf[1] << 8) | buf[2];

    return value;
}

void setup() {
    if (wiringPiSetup() == -1) {
        printf("Failed to initialize wiringPi library\n");
        exit(1);
    }

    pinMode(FACE_SENSOR_PIN, INPUT);
    pinMode(BUTTON_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
}

void turn_on_led() {
    digitalWrite(LED_PIN, HIGH);
}

void turn_off_led() {
    digitalWrite(LED_PIN, LOW);
}

bool button_pressed() {
    if (digitalRead(BUTTON_PIN) == LOW) {  // 버튼 눌림 감지
        delay(200);  // 디바운스 처리
        while (digitalRead(BUTTON_PIN) == LOW); // 손 뗄 때까지 대기
        return true;
    }
    return false;
}

void activate_buzzer() {
    digitalWrite(BUZZER_PIN, HIGH);
    printf("🔔 부저 작동 (2초)\n");
    delay(20000); // 2초 울림
    digitalWrite(BUZZER_PIN, LOW);
}

int read_distance() {
    long start_time, end_time;
    float distance;

    // TRIG 펄스 보내기
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10); // 최소 10μs
    digitalWrite(TRIG_PIN, LOW);

    // ECHO 핀에서 HIGH 신호 대기 (신호 시작 시간 측정)
    while (digitalRead(ECHO_PIN) == LOW);
    start_time = micros();

    // ECHO 핀 LOW 될 때까지 대기 (신호 끝 시간 측정)
    while (digitalRead(ECHO_PIN) == HIGH);
    end_time = micros();

    // 왕복 시간 → 거리 계산 (초음파 속도 = 340 m/s)
    long travel_time = end_time - start_time;
    distance = travel_time / 58.0; // cm 단위 (왕복)

    return (int)distance;
}

bool is_user_focused() {
    static int prev_distance = -1;
    int current_distance = read_distance(); // 센서에서 거리 측정 (ex. VL53L0X, HC-SR04)
    
    if (current_distance < distance_min || current_distance > distance_max) {
        return false;  // 너무 가까움 또는 너무 멂
    }

    if (prev_distance == -1) {
        prev_distance = current_distance;
        return true;
    }

    int diff = abs(current_distance - prev_distance);
    prev_distance = current_distance;

    if (diff <= stable_threshold) {
        return true; // 거의 움직이지 않음 → 집중
    } else {
        return false; // 움직임 있음 → 불집중
    }
}

void read_environment(int* temperature, int* humidity, int* noise) {
    // 온도 측정 (ADC 기반)
    int temp_adc = readADC(TEMP_SENSOR_CH);  // 예: 0~1023
    *temperature = temp_adc * 100 / 1023;    // 정규화: 0~100도 범위로 환산

    // 습도 측정 (ADC 기반)
    int humid_adc = readADC(HUMID_SENSOR_CH);  // 예: 0~1023
    *humidity = humid_adc * 100 / 1023;        // 정규화: 0~100%

    // 소음 측정
    int noise_adc = readADC(SOUND_SENSOR_CH);  // 예: 0~1023
    *noise = noise_adc / 10;                   // 정규화: 0~102 정도
}

void record_entry_time() {
    time_t now = time(NULL);             // 현재 시간 가져오기
    struct tm* t = localtime(&now);      // 현지 시간 구조체로 변환

    printf("🕒 입장 시각: %04d-%02d-%02d %02d:%02d:%02d\n",
        t->tm_year + 1900,
        t->tm_mon + 1,
        t->tm_mday,
        t->tm_hour,
        t->tm_min,
        t->tm_sec);
}

void generate_focus_report() {
    double focus_score = focus_time_min - (noise_time_min * 0.5);

    double avg_temp = env_sample_count ? (double)temp_sum / env_sample_count : 0;
    double avg_humid = env_sample_count ? (double)humid_sum / env_sample_count : 0;
    double avg_noise = env_sample_count ? (double)noise_sum / env_sample_count : 0;

    printf("\n📄 집중 리포트\n--------------------------\n");
    printf("🕓 총 집중 시간 : %d 분\n", focus_time_min);
    printf("🔊 소음 감지 시간 : %d 분\n", noise_time_min);
    printf("📈 집중 점수 : %.1f 점\n", focus_score);
    printf("\n🌡 평균 온도 : %.1f°C\n", avg_temp);
    printf("💧 평균 습도 : %.1f%%\n", avg_humid);
    printf("🔊 평균 소음 : %.1f\n", avg_noise);
    printf("--------------------------\n\n");

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
    } else {
        printf("❌ 리포트 파일 저장 실패\n");
    }
}

int main() {
    setup();

    while (1) {
        // 1. 얼굴 인식
        user_authenticated = face_recognition();
        if (!user_authenticated) {
            printf("❌ 인증 실패. 입장 거부됨.\n");
            continue;
        }
        record_entry_time();
        printf("✅ 얼굴 인증 성공\n");

        printf("📌 버튼을 눌러 집중 모드를 시작하세요.\n");

        // 버튼 누를 때까지 대기
        while (!button_pressed());

        // 집중 모드 시작
        focus_mode = true;
        focus_time_min = 0;
        noise_time_min = 0;
        turn_on_led();
        printf("🎯 집중 모드 시작\n");

        while (focus_mode) {
            int focus_count = 0;
            int noise_count = 0;
            int unfocused_count = 0; // 연속 이탈 횟수 초기화

            // 1분 측정 루프
            for (int i = 0; i < 60; i++) {
                if (is_user_focused()) {
                    focus_count++;
                    unfocused_count = 0; // 집중하면 초기화
                } else {
                    unfocused_count++;
                    if (unfocused_count == 10) {
                        activate_buzzer(); // 10초 연속 불집중 시 부저 울림
                        notify_admin("⚠ 집중 상태 10회 연속 이탈 감지됨");
                    }
                }

                // 환경 센서 읽기
                int temp, humid, noise;
                read_environment(&temp, &humid, &noise);
                temp_sum += temp;
                humid_sum += humid;
                noise_sum += noise;
                env_sample_count++;

                bool env_issue = false;
                char admin_msg[256] = "환경 경고 - ";
                char user_msg[256] = "⚠ 환경 상태 이상: ";

                if (temp > 30) {
                    strcat(admin_msg, "온도 초과 ");
                    strcat(user_msg, "온도↑ ");
                    env_issue = true;
                }
                if (humid > 70) {
                    strcat(admin_msg, "습도 초과 ");
                    strcat(user_msg, "습도↑ ");
                    env_issue = true;
                }
                if (noise > 70) {
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

                // 중간에 버튼 누르면 집중 모드 종료
                if (button_pressed()) {
                    focus_mode = false;
                    break;
                }
            }

            // 1분 측정 결과 반영
            if (focus_mode) { // 중간 종료된 경우 제외
                if (focus_count >= 45) focus_time_min++;
                if (noise_count >= 20) noise_time_min++;
            }
        }

        // 집중 모드 종료 후 처리
        turn_off_led();
        printf("🛑 집중 모드 종료됨\n");

        // 리포트 출력
        generate_focus_report();
        printf("📥 다음 사용자 대기 중...\n\n");
    }

    return 0;
}