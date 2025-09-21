#include <Arduino.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>
#include "boards.h"
#include <SD.h>

const char* ssid = "CarrancaTarget2025";
const char* password = "";

WebServer server(80);

#define SDCARD_CS 13

// coordenadas
uint8_t hora;
uint8_t minuto;
float latitude;
float longitude;

bool dadosRecebidos = false;

// Substitua a sua função handleRoot por esta
// No código da sua placa RECEPTORA (Lilygo T-Display com Wi-Fi)

// Substitua a sua função handleRoot por esta versão corrigida
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Carranca 2025 - Histórico</title>
  <meta charset="UTF-8">  <!-- <<<<<<< 1. ADICIONADO AQUI -->
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    /* ... seu CSS continua o mesmo ... */
    body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f4f4f4; }
    h1, h2 { text-align: center; color: #333; }
    #container { max-width: 800px; margin: 0 auto; background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    .coordenada { border-bottom: 1px solid #eee; padding: 10px 0; }
    .coordenada:last-child { border-bottom: none; }
    .coordenada p { margin: 5px 0; }
    .coordenada.ultima { background-color: #e8f4ff; padding: 15px; border-radius: 5px; }
    strong { color: #0056b3; }
  </style>
  <script>
  function decimalParaDMS(decimal, isLatitude) {
    const direcoes = isLatitude ? ['N', 'S'] : ['E', 'W'];
    const direcao = direcoes[decimal < 0 ? 1 : 0];
    
    const absDecimal = Math.abs(decimal);
    const graus = Math.floor(absDecimal);
    const minutosDecimal = (absDecimal - graus) * 60;
    const minutos = Math.floor(minutosDecimal);
    const segundos = ((minutosDecimal - minutos) * 60).toFixed(2);
    
    // O caractere de grau já está aqui, e agora será renderizado corretamente
    return graus + '° ' + minutos + "'" + segundos + '"' + direcao;
  }

  // ... o resto do seu JavaScript continua o mesmo ...
  function atualizarDados() {
    fetch('/dados')
      .then(response => response.json())
      .then(data => {
        const container = document.getElementById('historico');
        if (!data.recebido || data.coordenadas.length === 0) {
          container.innerHTML = "<p><strong>Nenhuma coordenada recebida</strong></p>";
          return;
        }

        const coordenadas = data.coordenadas.reverse();
        let html = '<h2>Histórico de Coordenadas</h2>';

        coordenadas.forEach((coord, index) => {
          const latDMS = decimalParaDMS(coord.latitude, true);
          const lonDMS = decimalParaDMS(coord.longitude, false);
          
          const classe = (index === 0) ? 'coordenada ultima' : 'coordenada';

          html += `<div class="${classe}">`;
          if (index === 0) {
            html += '<p><strong>ÚLTIMA COORDENADA RECEBIDA</strong></p>';
          }
          html += '<p><strong>Horário:</strong> ' + coord.hora + ':' + coord.minuto + '</p>' +
                  '<p><strong>Latitude:</strong> ' + latDMS + '</p>' +
                  '<p><strong>Longitude:</strong> ' + lonDMS + '</p>' +
                  '<p><a href="https://www.google.com/maps?q=' + coord.latitude + ',' + coord.longitude + '" target="_blank">Ver no Google Maps</a></p>' +
                  '</div>';
        } );
        
        container.innerHTML = html;
      })
      .catch(error => {
        console.error('Erro ao buscar dados:', error);
        document.getElementById('historico').innerHTML = "<p>Erro ao carregar dados. Tentando novamente...</p>";
      });
  }

  setInterval(atualizarDados, 5000);
  window.onload = atualizarDados;
  </script>
</head>
<body>
  <div id="container">
    <h1>Carranca 2025</h1>
    <div id="historico">
      <p>Carregando histórico de coordenadas...</p>
    </div>
  </div>
</body>
</html>
)rawliteral";
  server.send(200, "text/html; charset=UTF-8", html);
}


// Substitua a sua função handleJSON por esta
void handleJSON() {
  File file = SD.open("/coordenadas.csv", FILE_READ);
  String json = "{\"recebido\": " + String(dadosRecebidos ? "true" : "false") + ", \"coordenadas\": [";
  
  if (file && dadosRecebidos) {
    bool first = true;
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (line.length() > 0) {
        if (!first) {
          json += ",";
        }
        
        // Formato esperado: HH:MM,latitude,longitude
        int timeColon = line.indexOf(':');
        int firstComma = line.indexOf(',');
        int secondComma = line.indexOf(',', firstComma + 1);

        String horaStr = line.substring(0, timeColon);
        String minStr = line.substring(timeColon + 1, firstComma);
        String latStr = line.substring(firstComma + 1, secondComma);
        String lonStr = line.substring(secondComma + 1);

        json += "{";
        json += "\"hora\":\"" + horaStr + "\",";
        json += "\"minuto\":\"" + minStr + "\",";
        json += "\"latitude\":" + latStr + ",";
        json += "\"longitude\":" + lonStr;
        json += "}";
        
        first = false;
      }
    }
    file.close();
  }
  
  json += "]}";
  server.send(200, "application/json", json);
}


void setup() {
  initBoard();
  // When the power is turned on, a delay is required.
  delay(1500);

  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);

  if (!SD.begin(SDCARD_CS)) {
    Serial.println("Erro ao iniciar o cartão SD!");
  } else {
    Serial.println("Cartão SD iniciado com sucesso.");
  }

  if (!LoRa.begin(LoRa_frequency)) {
    Serial.println("SAR Start Error!");
    while (1);
  }

  // Configuração de potencia do LORA
  LoRa.setSyncWord(0x34);
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(8);
  LoRa.enableCrc();

  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/dados", handleJSON);
  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  server.handleClient();  // mantém servidor funcionando

  if (packetSize > 0) {
    const int COORDENADA_TAMANHO = 10;
    uint8_t buffer[packetSize];
    int i = 0;

    while (LoRa.available() && i < packetSize) {
      buffer[i++] = (uint8_t)LoRa.read();
    }

    // Imprime todo o conteúdo do pacote recebido
    Serial.printf("Pacote recebido (%d bytes):\n", packetSize);
    for (int j = 0; j < packetSize; j++) {
      Serial.printf("%02X ", buffer[j]);
    }
    Serial.println();

    int totalCoordenadas = packetSize / COORDENADA_TAMANHO;

    if (totalCoordenadas >= 1) {
      // apaga no SD
      SD.remove("/coordenadas.csv");

      for (int c = 0; c < totalCoordenadas; c++) {
        int offset = c * COORDENADA_TAMANHO;
        uint8_t h = buffer[offset];
        uint8_t m = buffer[offset + 1];
        float lat, lon;
        memcpy(&lat, &buffer[offset + 2], sizeof(float));
        memcpy(&lon, &buffer[offset + 6], sizeof(float));

        // salva SD
        File file = SD.open("/coordenadas.csv", FILE_APPEND);
        if (file) {
          file.printf("%02d:%02d,%.6f,%.6f\n", h, m, lat, lon);
          file.close();
        }

        // Atualiza para mostrar a última coordenada no servidor
        if (c == totalCoordenadas - 1) {
          hora = h;
          minuto = m;
          latitude = lat;
          longitude = lon;
          dadosRecebidos = true;

          Serial.printf("Hora: %02d\n", hora);
          Serial.printf("Minuto: %02d\n", minuto);
          Serial.printf("Latitude: %.6f\n", latitude);
          Serial.printf("Longitude: %.6f\n", longitude);
        }
      }

      Serial.println("Todas as coordenadas foram salvas.");
    }
  }
}