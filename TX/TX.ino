#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <MQUnifiedsensor.h>
#include <Preferences.h>

// === Pin LoRa ===
#define LORA_SCK  36
#define LORA_MISO 37
#define LORA_MOSI 35
#define LORA_SS   38
#define LORA_RST  40
#define LORA_DIO0 39

// ===Pin BME-280===
#define BME_SCK  0 // SCL
#define BME_MOSI 45 // SDA
#define BME_CS   48 // CSB
#define BME_MISO 47 // SDO

// ===MQ135 setup==
#define Board "ESP32"
#define Voltage_Resolution 3.3
#define ADC_Bit_Resolution 12
#define MQ_PIN 9
#define type "MQ-135"
#define RatioMQ135CleanAir 3.6

// === Sea Level ===
#define SEALEVELPRESSURE_HPA (1013.25)

MQUnifiedsensor MQ135(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ_PIN, type);
Preferences prefs;

Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // SPI

void setup() 
{
  Serial.begin(115200);
  delay(1000); // piccola pausa opzionale
  if (Serial) 
    Serial.println("Serial attiva");
  
  // === Inizializzazione SPI e LoRa === 
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("Errore inizializzazione LoRa!");
    while (1);
  }
  Serial.println("LoRa inizializzato con successo!");

  // === Inizializzazione BME 280 ====
  Serial.println("Inizializzazione BME280 via SPI...");
  if (!bme.begin()) 
  {
    Serial.println("Errore: sensore BME280 non trovato!");
    while (1);
  }
  Serial.println("Sensore BME280 inizializzato correttamente.");

  // === MQ135 setup ===
  MQ135.setRegressionMethod(1); // ppm = a * ratio^b
  MQ135.setA(110.47); MQ135.setB(-2.862); // per CO2
  MQ135.init();

  prefs.begin("mq135", false);
  float r0 = prefs.getFloat("r0", 0.0);

  if (r0 == 0.0) 
  {
    Serial.println("Calibrazione...");
    Serial.println("Calibrazione in corso (20s, aria pulita)...");
    for (int i = 0; i < 20; i++) 
    {
      MQ135.update();
      MQ135.calibrate(RatioMQ135CleanAir);
      delay(1000);
    }
    r0 = MQ135.getR0();
    prefs.putFloat("r0", r0);
    Serial.println("R0 salvato!");
    Serial.println("Calibrazione completata.");
    // LED
    delay(2000);
  }

  MQ135.setR0(r0);
  prefs.end();

  Serial.println("MQ135 pronto!");
  delay(1500);

}

void loop() {
  
  // Leggi temperatura e umidità pressione
  float temperatura = bme.readTemperature();
  float umidita = bme.readHumidity();
  float pressione = bme.readPressure();
  float altitudine = bme.readAltitude(SEALEVELPRESSURE_HPA);
  float ppm = MQ135.readSensor();

  // === Serial Print ===
  Serial.print("Temperatura = ");
  Serial.print(bme.readTemperature());
  Serial.println(" °C");

  Serial.print("Pressione = ");
  Serial.print(bme.readPressure() / 100.0F);
  Serial.println(" hPa");

  Serial.print("Altitudine approssimativa = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  Serial.print("Umidità = ");
  Serial.print(bme.readHumidity());
  Serial.println(" %");
  
  MQ135.update();
  Serial.print("CO2 stimata: ");
  Serial.print(ppm);
  Serial.println(" ppm");
  Serial.println("-----------------------------");
  
  // === LoRa Send ===
  LoRa.beginPacket();
  LoRa.print("Temp: ");
  LoRa.print(temperatura);
  LoRa.print(" C   Um: ");
  LoRa.print(umidita);
  LoRa.print(" %     ");
  LoRa.print("Pres: ");
  LoRa.print(pressione / 100.0F, 2);
  LoRa.print("hPa");
  LoRa.print("Alt: ");
  LoRa.print(altitudine);
  LoRa.print(" m");
  LoRa.print("    CO2: ");
  LoRa.print(ppm);
  LoRa.print("ppm");
  if (ppm < 400) 
    LoRa.print("      Aria ottima     ");
  else if (ppm < 1000) 
    LoRa.print("      Aria moderata   ");
  else if (ppm < 2000) 
    LoRa.print("      Aria scarsa     ");
  else 
    LoRa.print("      Aria pessima!   ");
  LoRa.endPacket();

  delay(1000); // invia ogni 1s 
}
