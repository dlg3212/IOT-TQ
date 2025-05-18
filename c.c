#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include "bt_master.h"

#define TEMP_THRESHOLD 30
#define HUMID_THRESHOLD 60
#define NOISE_THRESHOLD 50

#define SPI_CH 0
#define ADC_CH1 
#define ADC_CH2 
#define ADC_CS 29
#define SPI_SPEED 500000

#define study_start_switch
#define LED
#define distance
#define buzzer
#define temperature
#define humidity
#define camera
#define sound_recognize


bool authorized = false; // 얼굴 인증 여부
bool focus_mode = false;


// ADC -> D
int readADC(int adcChannel) {
    unsigned char buf[3];
    int value;

    buf[0] = 0x06 | ((adcChannel & 0x04) >> 2);
    buf[1] = ((adcChannel & 0x03) << 6);
    buf[2] = 0x00;
    digitalWrite(ADC_CS, 0);
    wiringPiSPIDataRW(SPI_CH, buf, 3);
    buf[1] = 0x0F & buf[1];
    value = (buf[1] << 8) | buf[2];
     digitalWrite(ADC_CS, 1);

    return value;
}


void recognize_face() {
    int face_id;
    printf("얼굴 ID를 입력하세요 (등록된 사용자만 허용): ");
    scanf("%d", &face_id);
    if (face_id == 1) {
        authorized = true;
        time_t now = time(NULL);
        printf("출입 인증 완료: %s", ctime(&now));
    } else {
        authorized = false;
        printf("인증 실패: 센서 비활성화 상태입니다.\n");
    }
}

void check_environment(int temp, int humid, int noise) {
    if (!authorized) return;

    if (temp > TEMP_THRESHOLD || humid > HUMID_THRESHOLD || noise > NOISE_THRESHOLD) {
        printf("환경 경고 발생!\n");
        if (temp > TEMP_THRESHOLD) printf("▶ 온도 높음: %d도\n", temp);
        if (humid > HUMID_THRESHOLD) printf("▶ 습도 높음: %d%%\n", humid);
        if (noise > NOISE_THRESHOLD) printf("▶ 소음 높음: %ddB\n", noise);
        printf("→ 관리자/사용자에게 메시지 전송됨\n");
    }
}

void check_focus(int distances[], int len) {
    if (!authorized || !focus_mode) return;

    int stable_count = 0;
    for (int i = 1; i < len; i++) {
        if (abs(distances[i] - distances[i - 1]) < 2) {
            stable_count++;
        }
    }

    if (stable_count == len - 1) {
        printf("집중 상태 유지 중입니다.\n");
    } else {
        printf("집중 상태 이탈 감지됨 → 부저 작동 및 관리자에게 알림 전송\n");
    }
}

void toggle_study_mode(bool on) {
    focus_mode = on;
    if (on) {
        printf("공부 시작: 조명 ON\n");
    } else {
        printf("공부 종료: 조명 OFF\n");
    }
}

void generate_report(int focus_min, int noise_min) {
    int score = focus_min - (noise_min * 0.5);
    if (score < 0) score = 0;

    printf("\n[집중 피드백 리포트]\n");
    printf("오늘 집중 시간: %d분\n", focus_min);
    printf("소음 노출 시간: %d분\n", noise_min);
    printf("집중 지수: %d점\n", score);
}

int main() {

    int client = init_server();

    char *send_message;

    if(wiringPiSetup() == -1) return -1;

    if(wiringPiSPISetup() == -1) return -1;
    
    send_message = 


    recognize_face();

    while(){
        toggle_study_mode(true);

        check_environment(32, 65, 55);  // 예시: 온도/습도/소음 입력

        int distances[5] = {30, 30, 30, 30, 30};  // 고정 거리 → 집중 상태

        check_focus(distances, 5);

        toggle_study_mode(false);

        generate_report(60, 20);

        return 0;
    }
}





