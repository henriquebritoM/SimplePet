/* Fill-in information from Blynk Device Info here */
#define BLYNK_TEMPLATE_ID           "TMPL2Sh5xLf-Z"
#define BLYNK_TEMPLATE_NAME         "Henrique de Brito Melo Silva"
#define BLYNK_AUTH_TOKEN            "t1TmIXag8GJjCFeHcrgZDFyGSzzdrP82"

#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp32.h>
#include "BasicStepperDriver.h"

#define MOTOR_STEPS 200
#define RPM 25

#define DIR 19
#define STEP 18
BasicStepperDriver stepper(MOTOR_STEPS, DIR, STEP);
bool is_stepper_on = 0;

#define pin_button 4
LiquidCrystal_I2C lcd(0x27, 16, 2);

enum class State {
  /*
  Idle,
  Feeding,
  Configuring
  */
  No_WIFI, 
  W_WIFI
};


enum class TimeParameter {
  WeekDay,
  Month,
  Day,
  Year,
  Hour,
  Minute,
  Second,
  HourMin,
  All
};

//RTC means the value will not be erased upon deep sleep  // RTC serve para não perder os valores depois do esp entrar em deep sleep
//RTC_DATA_ATTR char feeder_alarms[7][6] = { "21:10", "21:11", "21:12", "21:13", "21:14", "21:15" };
//RTC_DATA_ATTR int boot_count = 0;
Preferences preferences;  // biblioteca permite salvar dados de modo que não se percam mesmo após queda de energia

RTC_DATA_ATTR State current_state = State::No_WIFI;

//RTC_DATA_ATTR bool is_time_sync = false;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -60 * 60 * 3;

String ssid = " ";
String password = " ";

String last_min = "100";
unsigned long millis_last_update = millis();

volatile bool button_state = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting Setup.\n");
  pinMode(pin_button, INPUT);

  lcd.init();
  lcd.backlight();

  attachInterrupt(digitalPinToInterrupt(pin_button), ButtonInterrupt, CHANGE);

  stepper.begin(RPM);
  //stepper.setSpeedProfile(, 10, 10);

  // create the namespace for the wifi configs  // cria o lugar para salvar os dados 
  preferences.begin("credentials", false);

  ssid = preferences.getString("ssid", " ");
  password = preferences.getString("password", " ");

  Serial.print(ssid);
  Serial.print("  ||  ");
  Serial.print(password);
  Serial.println();

  preferences.end();

  update_wifi_status();
  update_state();

  switch (current_state)
  {
    case State::W_WIFI:
      Blynk.begin(BLYNK_AUTH_TOKEN, ssid.c_str(), password.c_str()); 
    break;

    case State::No_WIFI:
      Serial.println("Sem conexão! Iniciando configuração de wifi");
    break;
  }

  update_display();


  //wifi_configured = preferences.getBool("wifi_config", 0);

  //configure wifi if not already configured
  /*
  switch (current_state)
  {
    case State::No_Wifi:
      display_no_wifi();
      if (!try_connect_wifi())  {ESP.restart();}
      break;
    case State::W_Wifi:
      display_w_wifi();
      if (!try_connect_wifi()) 
      {
        current_state = State::No_Wifi;
        ESP.restart();
      }
      break;
  }

  // sincronizar com o horário real
  int failed_sync_tries = 0;
  while (!is_time_sync) {
    get_NTP();
    if (!is_time_sync)
    {
      failed_sync_tries++;
    }
    if (failed_sync_tries >= 5)
    {
      current_state = State::No_Wifi;
      ESP.restart();
    }
  }
  */

}

void loop() {
  Blynk.run();

  if (is_stepper_on && stepper.getCurrentState() == BasicStepperDriver::State::STOPPED) 
  {
    motor_move();
  }

  if (button_state == 1)// espera o usuário apertar o botão
  {
    Serial.println("INICIANDO A REDE PET!");
    config_wifi();
    update_state();
    update_display();
    while(digitalRead(pin_button) == 1) {}
    ESP.restart();
  }

  //Serial.println(millis_last_update - millis());
  if ((millis() - millis_last_update) < 60000)  // waits one minute
  {
  return;
  }

  update_wifi_status();
  update_state();
  update_display();

  millis_last_update = millis();

}
/*
    // coloca o esp para dormir, economizando muita energia
    esp_sleep_enable_ext1_wakeup(pin_button_mask, ESP_EXT1_WAKEUP_ANY_HIGH);
    esp_sleep_enable_timer_wakeup((60 - get_time(TimeParameter::Second).toInt()) * 1000000);  // get the time until next minute to sleep  // coloca para dormir até o próximo minuto
    Serial.flush();
    esp_deep_sleep_start();

*/
// Tries to connect - if fail 
void config_wifi()
{
  bool res;
  WiFiManager wm;    

  bool connected = false;
  while (!connected)

  {
    res = wm.autoConnect("Pet");

    if(!res) {
      Serial.println("Failed to connect or hit timeout");
    } 
    else {
      Serial.println(wm.getWiFiSSID() + " || " + wm.getWiFiPass());
      
      // update ssid and password and saves them into prefences
      ssid = wm.getWiFiSSID();
      password = wm.getWiFiPass();

      preferences.begin("credentials", false);

      preferences.putString("ssid", ssid);
      preferences.putString("password", password);

      preferences.end();
    }
    wm.disconnect();

    connected = try_to_connect_wifi();  // if conects, connect == true
  }
}


// sincroniza com o horário real
void get_NTP() {
  Serial.println("Start NTP fn...\n");
  
  //is_time_sync = true;
  Serial.println("Sync NTP...");
  sync_real_time();  // May set is_time_sync to false if not possible to sync.

}

void sync_real_time() {
  //init and get the time
  print_local_time();
}

void print_local_time() {
  TimeParameter i = TimeParameter::All;
  Serial.println(get_time(i));
}

// retorna o horário, em diferentes formatos, todos string
String get_time(TimeParameter parameter) {
  
  configTime(gmtOffset_sec, 0, ntpServer);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
   //is_time_sync = false;
    return "";
  }

  String wday_name[7] = { "Domingo", "Segunda-Feira", "Terça-Feira", "Quarta-Feira", "Quinta-Feira", "Sexta-Feira", "Sábado" };
  String months_name[12] = { "Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho", "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro" };

  String i;
  switch (parameter) {
    case TimeParameter::Day:
      i = String(timeinfo.tm_mday);
      return i;
      break;

    case TimeParameter::Hour:
      i = fmt_int(timeinfo.tm_hour);
      return i;
      break;

    case TimeParameter::Minute:
      i = fmt_int(timeinfo.tm_min);
      return i;
      break;

    case TimeParameter::Second:
      i = fmt_int(timeinfo.tm_sec);
      return i;
      break;

    case TimeParameter::Month:
      i = months_name[timeinfo.tm_mon];
      return i;
      break;

    case TimeParameter::WeekDay:
      i = wday_name[timeinfo.tm_wday];
      return i;
      break;

    case TimeParameter::Year:
      i = fmt_int(timeinfo.tm_year + 1900);
      return i;
      break;

    case TimeParameter::HourMin:
      i = fmt_int(timeinfo.tm_hour) + ":" + fmt_int(timeinfo.tm_min);
      return i;

    case TimeParameter::All:
    
      String d = fmt_int(timeinfo.tm_mday);
      String h = fmt_int(timeinfo.tm_hour);
      String m = fmt_int(timeinfo.tm_min);
      String s = fmt_int(timeinfo.tm_sec);

      i = wday_name[timeinfo.tm_wday] + " " + months_name[timeinfo.tm_mon] + " " + d + " " + (timeinfo.tm_year + 1900) + " " + h + ":" + m + ":" + s;
      return i;
      break;
  }
}

String fmt_int(int i) {
  String s;
  if (i > 9) {
    s = String(i);
    return s;
  }
  else 
  {
    s = "0" + String(i);
    return s;
  }
}

int fmt_str_to_int(String s)
{
  int i = atoi(s.c_str());
  return i;
}

void display_w_wifi() {
  if (get_time(TimeParameter::Minute) != last_min)
  {
    lcd.clear();

    String tnow = get_time(TimeParameter::HourMin);
    last_min = get_time(TimeParameter::Minute);

    lcd.setCursor(4, 0);
    lcd.print(tnow);
  } 
}
  
void display_no_wifi() {
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Aperte o botao");
  lcd.setCursor(0, 1);
  lcd.print("conecte-se a Pet");
}

bool try_to_connect_wifi() {

  int max_attempts = 10;  //  número de tentativas entes do sistema dar como ssid / password inválidos
  int number_attempts = 0;
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);

  //tenta conectar
  while (WiFi.status() != WL_CONNECTED && number_attempts <= max_attempts) {
    delay(1000);
    number_attempts++;
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {  
    Serial.println(" CONNECTED");
    return true;
  } 
  else {
    Serial.println("FAILED TO CONNECTED");
    WiFi.disconnect(true);
    return false;
  }
}

void ButtonInterrupt() 
{
  button_state = digitalRead(pin_button);
}

void update_wifi_status() 
{
  Serial.println("atualizando status...");
  
  try_to_connect_wifi();
  
}

void update_state()
{
  Serial.println("UPDATING STATE");
  switch (WiFi.status())
  {

    case WL_CONNECTED:
      current_state = State::W_WIFI;
      Serial.println("wifi on");
    break;  

    default:
      current_state = State::No_WIFI;
      Serial.println("wifi off");
    break;
  }
  
}

void update_display()
{

  Serial.println("UPDATING DISPLAY");
  switch (current_state)
  {
    case State::W_WIFI:

        display_w_wifi();

    break;

    case State::No_WIFI:

      Serial.println("conexão falhou!");
      display_no_wifi(); // pede pro usuário apertar o botão

    break;
  }
}

// This function will be called every time Slider Widget
// in Blynk app writes values to the Virtual Pin V1
BLYNK_WRITE(V1)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  if (pinValue == 1)
  {
    is_stepper_on = 1;
    Serial.println("Motor Ligado");
  }
  else if (pinValue == 0)
  {
    is_stepper_on = 0;
    Serial.println("Motor Desligado");
    motor_stop();
  }
}

void motor_move() 
{
  stepper.rotate(60);
}

void motor_stop()
{
  stepper.stop();
  stepper.disable();
}






