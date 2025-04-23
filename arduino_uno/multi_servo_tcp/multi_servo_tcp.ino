
// 25.04.23

// 모터 별 속도 조절 & "ON" 신호 전송 추가

// 1. 문열림 - 인형 앞면으로 모두 전환 후 "ON" 신호 전송
// 2. 문닫힘 - 서버에서 "OFF" 신호 수신 후 인형 뒷면으로 전환


#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Servo.h>
#include <Ethernet.h>

// ---------------- 네트워크 설정 ----------------
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 22);       // 아두이노IP (고정IP)
IPAddress serverIp(192, 168, 0, 21); // 서버 IP
int port = 12345;                    // 포트번호
EthernetClient client;

// ---------------- 핀 설정 ----------------
const int LED_TCP_PIN = 8;
const int INF_CW_PIN = 6; 
const int INF_CCW_PIN = 9;
const int W5500_RST_PIN = 5;
const int REED_SWITCH_PIN = 7; // 도어센서 핀

// ---------------- 서보 설정 ----------------
#define MOTOR_COUNT 16
#define SERVOFREQ 50

#define SERVOMIN 100
#define SERVOMAX 500

// ----------------모터 설정----------------
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
Servo infCw,infCcw;
#define MOTOR_START 0
#define MOTOR_END 16
const int INF_CW = 80;
const int INF_CCW = 100;
const int INF_STOP = 90;


// ---------------- 모터 속도 설정----------------
const int motorSpeeds[MOTOR_COUNT] = {
    0, 0, 0, 0, 0,                                   
    1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2};

bool motorStarted = false;        // 모터 상태 변수 (false: 인형 뒷면, true: 인형 앞면)
const int DOOR_OPEN_DELAY = 1000; // 문 열릴때 딜레이 시간(1초)

// ---------------- 함수 선언 ---------------------
void startMotors();
void resetMotors();
void setPWMForMotors(uint16_t start, uint16_t end, uint16_t value);
void blinkLED();

// ------------------------------------------------
void setup() {
  Serial.begin(9600);

  pinMode(REED_SWITCH_PIN, INPUT_PULLUP); // 도어센서
  pinMode(LED_TCP_PIN, OUTPUT);
  pinMode(W5500_RST_PIN, OUTPUT);         
  digitalWrite(W5500_RST_PIN, LOW);delay(100);
  digitalWrite(W5500_RST_PIN, HIGH);delay(100);

  infCw.attach(INF_CW_PIN); // 무한회전서보 핀연결
  infCcw.attach(INF_CCW_PIN);

  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVOFREQ);

  infCw.write(INF_STOP);
  infCcw.write(INF_STOP);

  Ethernet.begin(mac, ip);
  resetMotors(); //초기상태 : 인형 뒷면
}


// ------------------------------------------------
void loop() {

  // ----- 1.서버 연결 상태 확인 및 재연결 시도 -----
  if(!client.connected()) {
    Serial.println("서버에 연결 시도 중...");

    if(client.connect(serverIp, port)) {
      Serial.println("----서버에 연결 성공----");
      digitalWrite(LED_TCP_PIN, HIGH); // LED 켜기
    } else {
      Serial.println("서버에 연결 실패.5초후 재시도...");
      blinkLED();  // LED 점멸
      delay(5000); // 5초 대기 후 재시도
      return;
    }
  }

  // ----- 2. 문열림 >>> 인형앞면전환 & "ON" 신호 전송 -----
  if (digitalRead(REED_SWITCH_PIN) == HIGH && !motorStarted) {
    Serial.println("문열림- 1초후 모터 돌아감");
    delay(DOOR_OPEN_DELAY); // 1초 대기

    startMotors(); // 인형 앞면으로 전환
    motorStarted = true;
    
    if(client.connected()) {
      client.println("ON"); // 서버에 "ON" 신호 전송
      Serial.println("서버에 ON 신호 전송");
    } else {
      Serial.println("서버와 연결 끊김. 신호 전송 실패.");
    }
  }

  // ----- 3. 문닫힘 & "OFF"신호 수신 >>> 인형 뒷면 전환 -----
  if(digitalRead(REED_SWITCH_PIN) == LOW && motorStarted) {
    if(client.available()) {
      String command = client.readStringUntil('\n');
      command.trim(); 
      Serial.println("명령 수신 대기중...");
      Serial.print("Received command: ");Serial.println(command);

      if(command == "OFF") {
        if (motorStarted) {
          Serial.println("모터 원상태 : 인형 뒷면으로 전환");
          resetMotors(); // 인형 뒷면으로 전환
        }
        motorStarted = false;
      } else {
        Serial.println("서버신호 : OFF 아님");  
      }
      delay(200);
    }
  }
}

// ------------------------------------------------
void startMotors() {
  Serial.println(" ▶ 인형 앞면으로 전환");

  infCw.write(INF_CW); // 무한 회전 서보 회전 시작
  infCcw.write(INF_CCW);

  const int targetAngle = 180; // 목표 각도

  for (int angle = 0; angle < targetAngle; angle++) {
    for (int i = MOTOR_START; i < MOTOR_END; i++){
      int pulse = map(angle, 0, 180, SERVOMIN, SERVOMAX); //0~180을 100~500으로 변환
      pwm.setPWM(i, 0, pulse);
      delay(motorSpeeds[i]); // 각 모터 별 딜레이 적용
    }
  }

  // 무한 회전 서보 5초 회전 유지
  delay(5000);
  infCw.write(INF_STOP);
  infCcw.write(INF_STOP);

  Serial.println("모든 모터 회전 완료");
}
 
// ------------------------------------------------
void resetMotors() {
  Serial.println(" ▶ 인형 뒷면으로 전환");
  setPWMForMotors(MOTOR_START, MOTOR_END, SERVOMIN);
  infCw.write(INF_STOP);
  infCcw.write(INF_STOP);
}

// ------------------------------------------------
void setPWMForMotors(uint16_t start, uint16_t end, uint16_t value) {
  for (uint16_t servonum = start; servonum < end; ++servonum) {
    pwm.setPWM(servonum, 0, value);
  }
}

// ------------------------------------------------
void blinkLED() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_TCP_PIN, HIGH);
    delay(200);
    digitalWrite(LED_TCP_PIN, LOW);
    delay(200);
  }
}
