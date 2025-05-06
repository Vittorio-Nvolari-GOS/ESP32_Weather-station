#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>
#include <MQUnifiedsensor.h>
#include <Preferences.h>

// === Pin Display ===
#define I2C_SDA 17
#define I2C_SCL 18

// === Pin LoRa ===
#define LORA_SCK  36
#define LORA_MISO 37
#define LORA_MOSI 35
#define LORA_SS   38
#define LORA_RST  40
#define LORA_DIO0 39

// === Pin DHT ===
#define DHTPIN     4
#define DHTTYPE    DHT11
DHT dht(DHTPIN, DHTTYPE);

// === MQ135 setup ===
#define Board "ESP32"
#define Voltage_Resolution 3.3
#define ADC_Bit_Resolution 12
#define MQ_PIN 9
#define type "MQ-135"
#define RatioMQ135CleanAir 3.6

MQUnifiedsensor MQ135(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ_PIN, type);
Preferences prefs;

LiquidCrystal_I2C lcd(0x27,16,2);

String waitForLoRaPacket(unsigned long timeout = 5000) // questa funzione serve ad attendere per un tot di tempo il segnale
{
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String receivedData = "";
      while (LoRa.available()) {
        receivedData += (char)LoRa.read();

      }
      Serial.print("[LoRa] Ricevuto: ");
      Serial.print(receivedData);
      Serial.print("  [RSSI: ");
      Serial.print(LoRa.packetRssi());
      Serial.println(" dB]");
      
      return receivedData;
    }
    delay(10);
  }
  Serial.println("[LoRa] Timeout: nessun pacchetto ricevuto.");
  return ""; // Stringa vuota in caso di timeout
}

void setup() 
{
  Serial.begin(115200);
  delay(1000); // piccola pausa 
  if (Serial) 
    Serial.println("Serial attiva");
    
  // === Inizializza SPI ===
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);

  // === Set pin LoRa ===
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // === Avvia LoRa a 433 MHz ===
  if (!LoRa.begin(433E6)) 
  {
    Serial.println("Errore durante l'inizializzazione LoRa");
    while (1);
  }
  Serial.println("Ricevitore LoRa pronto");
    
  // === Inizializzazione Display ===
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL, 10000); 
  lcd.init(); 
  lcd.clear();
  lcd.backlight();
  lcd.setContrast(30); // Valore di contrasto (da 0 a 255)
  lcd.setCursor(0,0); 

  // === Inizializzazione DHT ===
  dht.begin();
  Serial.println("Inizializzazione DHT11...");
  delay(2000);


  // === MQ135 setup ===
  MQ135.setRegressionMethod(1); // ppm = a * ratio^b
  MQ135.setA(110.47); 
  MQ135.setB(-2.862); // per CO2
  MQ135.init();

  prefs.begin("mq135", false);
  float r0 = prefs.getFloat("r0", 0.0);

  if (r0 == 0.0) 
  {
    lcd.clear(); 
    lcd.print("Calibrazione...");
    Serial.println("Calibrazione in corso (20s, aria pulita)...");
    for (int i = 0; i < 20; i++) 
    {
      MQ135.update();
      MQ135.calibrate(RatioMQ135CleanAir);
      delay(1000);
    }
    r0 = MQ135.getR0();
    prefs.putFloat("r0", r0);
    lcd.clear(); 
    lcd.print("R0 salvato!");
    Serial.println("Calibrazione completata.");
    delay(2000);
  }

  MQ135.setR0(r0);
  prefs.end();

  lcd.clear();
  lcd.print("MQ135 pronto!");
  delay(1500);


}

void loop() 
{
  
  // === Legge temperatura e umidità e qualità dell'aria ===
  float temperatura = dht.readTemperature();
  float umidita = dht.readHumidity();
  MQ135.update();
  float ppm = MQ135.readSensor();

  // === Verifica letture valide ===
  if (isnan(temperatura) || isnan(umidita)) 
  {
    Serial.println("Errore nella lettura dal sensore DHT!");
    delay(2000);
    return;
  }
  
  // === Serial Print ===
  Serial.println("==== Info interne ====  ");
  Serial.print("Temp: ");
  Serial.print(temperatura);
  Serial.println(" C");
  Serial.print("Umidita: ");
  Serial.print(umidita);
  Serial.println(" %");
  Serial.print("CO2 stimata: ");
  Serial.print(ppm);
  Serial.println(" ppm");
  if (ppm < 400) 
    Serial.println("Aria ottima     ");
  else if (ppm < 1000) 
    Serial.println("Aria moderata   ");
  else if (ppm < 2000) 
    Serial.println("Aria scarsa     ");
  else 
    Serial.println("Aria pessima!   ");
  Serial.println("======================");

  // === LCD Print ===
  lcd.setCursor(0, 0);
  lcd.print("  Info interne  ");
  delay(2000);
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperatura);
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("Umidita: ");
  lcd.print(umidita);
  lcd.print(" %");
  
  delay(2000);      
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CO2:");
  lcd.print((int)ppm);
  lcd.print(" ppm");

  lcd.setCursor(0, 1);
  if (ppm < 400) 
    lcd.print("Aria ottima     ");
  else if (ppm < 1000) 
    lcd.print("Aria moderata   ");
  else if (ppm < 2000) 
    lcd.print("Aria scarsa     ");
  else 
    lcd.print("Aria pessima!   ");

  delay(3000);
  lcd.clear();
  
  // === Parte di ricezione LoRa ===
  String msg = waitForLoRaPacket(2000);
      
  lcd.setCursor(0, 0);
  lcd.print("  Info esterne  ");
  delay(2000);
  lcd.clear();
  
  // Mostra i primi 16 caratteri sulla prima riga
  lcd.setCursor(0, 0);
  lcd.print(msg.substring(0, 16));
  // Mostra i successivi 16 caratteri sulla seconda riga
  lcd.setCursor(0, 1);
  lcd.print(msg.substring(16, 32));
  delay(2000);
  // Mostra i secondi 16 caratteri sulla prima riga
  lcd.setCursor(0, 0);
  lcd.print(msg.substring(32, 48));
  // Mostra i successivi 16 caratteri sulla seconda riga
  lcd.setCursor(0, 1);
  lcd.print(msg.substring(48, 64));
  delay(2000);
  // Mostra i terzi 16 caratteri sulla prima riga
  lcd.setCursor(0, 0);
  lcd.print(msg.substring(64, 80));
  // Mostra i successivi 16 caratteri sulla seconda riga
  lcd.setCursor(0, 1);
  lcd.print(msg.substring(80, 96));
  delay(5000);
  lcd.clear();
  
}