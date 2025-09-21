#include <Arduino.h>
#include "board.h"
#include <TinyGPS++.h>
#include <LoRa.h>
#include <vector>

TinyGPSPlus gps;

struct Coordenada {
  uint8_t hora;
  uint8_t minuto;
  float latitude;
  float longitude;
};

std::vector<Coordenada> coordenadas;

// %%%%%%%%%%% Codigo de Teste
const unsigned long intervaloGPS = 5 * 60 * 1000UL;
const unsigned long intervaloTransmissao = 1 * 60 * 1000UL;
const unsigned long intervaloEntreEnvios = 20000;

// ########### Código de Producao
// const unsigned long intervaloGPS = 5 * 60 * 1000UL;
// const unsigned long intervaloTransmissao = 15 * 60 * 1000UL;
// const unsigned long intervaloEntreEnvios = 20 * 1000UL;


enum Estado {
  GPS,
  TRANSMISSAO
};

Estado estado = GPS;

unsigned long atualizaTempoDeVida = 0;


void setup() {
  initBoard();

  pinMode(25, INPUT);

  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);

  if (!LoRa.begin(LoRa_frequency)) {
    Serial.println("Erro ao iniciar LoRa!");
    while (1);
  }

  // Configuração de potencia do LORA
  // LoRa.setSpreadingFactor(12);    // 12 Máxima sensibilidade
  // LoRa.setSignalBandwidth(125E3); // Banda estreita = maior alcance
  // //7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3, 41.7E3, 62.5E3, 125E3, and 250E3.
  // LoRa.setCodingRate4(8);         // Corrige mais erros
  // LoRa.setTxPower(20);            // Potência máxima permitida (dBm)

  LoRa.setSyncWord(0x34);
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(8);
  LoRa.setTxPower(20);
  LoRa.enableCrc();


  atualizaTempoDeVida = millis();
}

void loop() {
  unsigned long tempoDeVida = millis();

  // Verifica se a chave (pino 4) está ligada
  if (digitalRead(25) == LOW) {
    Serial.println("Sistema desativado");
    delay(1000); // Aguarda 1 segundo antes de verificar novamente
    return; // Sai do loop, não executa GPS nem Transmissão
  }

  switch (estado) {
    case GPS:
      // if (capturaGPS() || (tempoDeVida - atualizaTempoDeVida >= intervaloGPS)) {
      if (simulaGPS() || (tempoDeVida - atualizaTempoDeVida >= intervaloGPS)) {
        estado = TRANSMISSAO;
        atualizaTempoDeVida = tempoDeVida;
        Serial.println("Iniciando fase de transmissão.");
      }
      break;

    case TRANSMISSAO:
      if (tempoDeVida - atualizaTempoDeVida <= intervaloTransmissao) {
        tx();
        delay(intervaloEntreEnvios);
      } else {
        estado = GPS;
        atualizaTempoDeVida = tempoDeVida;
        Serial.println("Retornando para busca de GPS.");
      }
      break;
  }
}


bool simulaGPS() {

  while(1){
    if(true){
      if(true){
  
        Coordenada nova;
        nova.latitude = -25.0 + (rand() % 1000000) / 1000000.0;
        nova.longitude = -49.0 + (rand() % 1000000) / 1000000.0;
        nova.hora = random(0, 24);
        nova.minuto = random(0, 60);

        Serial.printf("Coordenada capturada: %02d:%02d, %.6f, %.6f\n", nova.hora, nova.minuto, nova.latitude, nova.longitude);

        coordenadas.push_back(nova);
        return true;

      }
    }
  }

  return false;

}

bool capturaGPS() {
// Código real com GPS:

  while (Serial1.available() > 0) {
    if (gps.encode(Serial1.read())) {
      if (gps.location.isValid()) {
        Coordenada nova;
        nova.latitude = gps.location.lat();
        nova.longitude = gps.location.lng();
        nova.hora = gps.time.hour();
        nova.minuto = gps.time.minute();

        coordenadas.push_back(nova);
        Serial.printf("Coordenada capturada: %02d:%02d, %.6f, %.6f\n", nova.hora, nova.minuto, nova.latitude, nova.longitude);
        return true;
      }
    }
  }

  return false;

}

void tx() {
  const int bytesPorCoord = 10;
  int totalCoord = coordenadas.size();
  int tamanhoPacote = totalCoord * bytesPorCoord;

  // Remove mais antigas se passar de 255 bytes
  while (tamanhoPacote > 255 && !coordenadas.empty()) {
    coordenadas.erase(coordenadas.begin());
    totalCoord = coordenadas.size();
    tamanhoPacote = totalCoord * bytesPorCoord;
  }

  if (totalCoord == 0) {
    Serial.println("Nenhuma coordenada para transmitir.");
    return;
  }

  uint8_t pacote[tamanhoPacote];

  for (int i = 0; i < totalCoord; i++) {
    pacote[i * 10 + 0] = coordenadas[i].hora;
    pacote[i * 10 + 1] = coordenadas[i].minuto;
    memcpy(&pacote[i * 10 + 2], &coordenadas[i].latitude, sizeof(float));
    memcpy(&pacote[i * 10 + 6], &coordenadas[i].longitude, sizeof(float));
  }

  // Envia por serial para debug
  Serial.printf("tr %d coordenadas (%d bytes):\n", totalCoord, tamanhoPacote);
  for (int i = 0; i < tamanhoPacote; i++) {
    Serial.print(pacote[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  LoRa.beginPacket();
  LoRa.write(pacote, tamanhoPacote);
  LoRa.endPacket();
}
