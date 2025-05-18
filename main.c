#include <stdio.h>
#include <wiringPi.h>
#include <stdbool.h>

#define FACE_SENSOR_PIN 0      // 예시 핀 번호
#define START_BUTTON_PIN 1
#define END_BUTTON_PIN 2
#define LED_PIN 3
#define BUZZER_PIN 4
#define DIST_SENSOR_PIN 5
#define TEMP_HUMID_SENSOR_CH 0
#define SOUND_SENSOR_CH 1

int readADC(int channel); // 외부 ADC 값 읽는 함수

bool recognize_face() {
    return digitalRead(FACE_SENSOR_PIN);  // 단순화된 얼굴 인식 시뮬레이션
}

bool is_button_pressed(int pin) {
    return digitalRead(pin) == LOW;  // 버튼 눌림 감지 (풀업 기준)
}

bool detect_focus() {
    int distance = readADC(DIST_SENSOR_PIN);
    return distance < 2000;  // 초음파 거리 기준으로 집중 판단
}

void monitor_environment(float* temp, float* humid, float* noise) {
    *temp = readADC(TEMP_HUMID_SENSOR_CH) * 0.1;
    *humid = readADC(TEMP_HUMID_SENSOR_CH) * 0.05;
    *noise = readADC(SOUND_SENSOR_CH) * 0.1;
}

void setup() {
    wiringPiSetup();
    pinMode(FACE_SENSOR_PIN, INPUT);
    pinMode(START_BUTTON_PIN, INPUT);
    pinMode(END_BUTTON_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(DIST_SENSOR_PIN, INPUT);

    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
}

int main() {
    setup();
    float temp, humid, noise;
    int focus_time = 0, noise_time = 0;

    printf("Waiting for face recognition...\n");
    while (!recognize_face()) {
        delay(500);
        printf("Face not recognized. Retry...\n");
    }
    printf("Face recognized. Entry recorded.\n");

    printf("Waiting for START button...\n");
    while (!is_button_pressed(START_BUTTON_PIN)) {
        delay(200);
    }

    printf("Study session started.\n");
    digitalWrite(LED_PIN, HIGH);

    while (1) {
        if (is_button_pressed(END_BUTTON_PIN)) break;

        if (detect_focus()) {
            focus_time += 1;
        } else {
            noise_time += 1;
            digitalWrite(BUZZER_PIN, HIGH);
            printf("Not focused! Admin alerted.\n");
            delay(1000);
            digitalWrite(BUZZER_PIN, LOW);
        }

        monitor_environment(&temp, &humid, &noise);
        if (temp > 30 || humid > 70 || noise > 60) {
            printf("Environment alert sent.\n");
        }

        delay(1000); // 1초 주기
    }

    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    float score = focus_time - (noise_time * 0.5f);
    printf("\nStudy session ended.\n");
    printf("Focus Time: %d s, Noise Time: %d s, Focus Score: %.2f\n", focus_time, noise_time, score);

    return 0;
}