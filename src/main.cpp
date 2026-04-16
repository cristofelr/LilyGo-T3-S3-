#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <RadioLib.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

// Pinos T3-S3
#define LORA_NSS 7
#define LORA_RST 8
#define LORA_DIO0 33
#define BOARD_PWR 38
#define DHTPIN 47 // Conecte o pino de dados do DHT21 aqui (IO47)
#define DHTTYPE DHT21

// WiFi
const char *ap_ssid = "T3S3-Monitor";
WebServer server(80);

// Hardware
Adafruit_SSD1306 display(128, 64, &Wire, 14);
DHT dht(DHTPIN, DHTTYPE);
String devEUI_str = "34B7DAFFFEDA08A0";

float temp = 0, hum = 0;

#include <LittleFS.h>

struct DataLog {
  float t;
  float h;
};
DataLog logs[60]; // Buffer rápido para as últimas leituras
int logIdx = 0;
int logCount = 0;

void saveToFS(float t, float h) {
  File file = LittleFS.open("/data.csv", FILE_APPEND);
  if (file) {
    file.printf("%.1f,%.1f\n", t, h);
    file.close();
  }
}

void handleRoot() {
  String view = server.hasArg("view") ? server.arg("view") : "hora";
  int page = server.hasArg("page") ? server.arg("page").toInt() : 0;

  // Lógica de dados para o gráfico (Mostra TODAS as disponíveis no buffer)
  String labels = "[", temp_data = "[", hum_data = "[";
  for (int i = 0; i < logCount; i++) {
    int idx = (logIdx - logCount + i + 60) % 60;
    labels += "'" + String(i + 1) + "'";
    temp_data += String(logs[idx].t, 1);
    hum_data += String(logs[idx].h, 1);
    if (i < logCount - 1) {
      labels += ",";
      temp_data += ",";
      hum_data += ",";
    }
  }
  labels += "]";
  temp_data += "]";
  hum_data += "]";

  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' "
                "content='width=device-width, initial-scale=1'>";
  html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  html += "<style>";
  html += "body { font-family: sans-serif; text-align: center; background: "
          "#121212; color: #eee; margin:0; padding: 10px;}";
  html += ".card { background: #1e1e1e; padding: 15px; margin-bottom: 15px; "
          "border-radius: 12px; border: 1px solid #333; }";
  html += ".btn { display: inline-block; padding: 8px 12px; background: #333; "
          "color: #fff; text-decoration: none; border-radius: 5px; margin: "
          "2px; font-size: 0.8em; }";
  html += ".active { background: #00d4ff; color: #000; }";
  html +=
      "table { width: 100%; border-collapse: collapse; font-size: 0.8em; } th, "
      "td { border: 1px solid #333; padding: 5px; text-align: center; }";
  html += "#chartContainer { height: 200px; width: 100%; }";
  html += "</style></head><body>";

  html += "<h3>T3-S3 MONITOR</h3>";

  html += "<div class='card' style='border-left: 5px solid #ff5555;'>";
  html += "<div style='color: #ff5555; font-weight: bold; font-size: "
          "1.2em;'>TEMPERATURA</div>";
  html += "<div class='val' style='color: #ff5555; font-size: 3em;'>" +
          String(temp, 1) + " °C</div></div>";

  html += "<div class='card' style='border-left: 5px solid #00d4ff;'>";
  html += "<div style='color: #00d4ff; font-weight: bold; font-size: "
          "1.2em;'>UMIDADE</div>";
  html += "<div class='val' style='color: #00d4ff; font-size: 3em;'>" +
          String(hum, 1) + " %</div></div>";

  html += "<div class='card'>";
  html += "<a href='/?view=hora' class='btn " +
          String(view == "hora" ? "active" : "") + "'>Hora</a>";
  html += "<a href='/?view=dia' class='btn " +
          String(view == "dia" ? "active" : "") + "'>Dia</a>";
  html += "<a href='/?view=mes' class='btn " +
          String(view == "mes" ? "active" : "") + "'>Mês</a>";

  html += "<div id='chartContainer'><canvas id='myChart'></canvas></div></div>";

  html += "<div "
          "class='card'><b>Histórico</b><table><tr><th>#</th><th>T</th><th>U</"
          "th></tr>";
  int start = page * 10;
  int end = start + 10;
  if (end > logCount)
    end = logCount;
  for (int i = start; i < end; i++) {
    int idx = (logIdx - 1 - i + 60) % 60;
    html += "<tr><td>" + String(logCount - i) + "</td><td>" +
            String(logs[idx].t, 1) + "</td><td>" + String(logs[idx].h, 1) +
            "</td></tr>";
  }
  html += "</table><br>";
  if (page > 0)
    html += "<a href='/?view=" + view + "&page=" + String(page - 1) +
            "' class='btn'><<</a>";
  if (end < logCount)
    html += "<a href='/?view=" + view + "&page=" + String(page + 1) +
            "' class='btn'>>></a>";
  html += "</div>";

  html += "<script>";
  html += "new Chart(document.getElementById('myChart'), { type: 'line', data: "
          "{ labels: " +
          labels + ", datasets: [";
  html += "{ label: 'T', data: " + temp_data +
          ", borderColor: '#ff5555', tension: 0.4 },";
  html += "{ label: 'U', data: " + hum_data +
          ", borderColor: '#5555ff', tension: 0.4 }";
  html += "]}, options: { responsive: true, maintainAspectRatio: false, "
          "plugins: { legend: { display: false } } } });";
  if (page == 0)
    html += "setTimeout(() => { location.reload(); }, 60000);";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  pinMode(BOARD_PWR, OUTPUT);
  digitalWrite(BOARD_PWR, HIGH);
  delay(100);

  Serial.begin(115200);
  Wire.begin(18, 17);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  dht.begin();

  LittleFS.begin(true); // true = formata se falhar

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("INICIANDO FS...");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin("CCLRZ", "Certing_123");
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 40) {
    delay(500);
    display.print(".");
    display.display();
    counter++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi: OK");
    display.println("IP: " + WiFi.localIP().toString());
    display.display();
    Serial.println("IP: " + WiFi.localIP().toString());
  } else {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi: Aguardando...");
    display.println("SSID: CCLRZ");
    display.display();
  }

  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();

  // WiFi Watchdog: reconecta à CCLRZ automaticamente se cair
  static uint32_t lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi caiu! Reconectando CCLRZ...");
      display.fillRect(0, 16, 128, 16, BLACK);
      display.setCursor(0, 16);
      display.println("WiFi: Reconectando");
      display.display();
      WiFi.disconnect();
      WiFi.begin("CCLRZ", "Certing_123");
      for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
      }
      if (WiFi.status() == WL_CONNECTED) {
        display.fillRect(0, 16, 128, 16, BLACK);
        display.setCursor(0, 16);
        display.println("IP: " + WiFi.localIP().toString());
        display.display();
        Serial.println("Reconectado! IP: " + WiFi.localIP().toString());
      }
    }
  }

  static uint32_t lastRead = 0;
  if (millis() - lastRead > 300000 || lastRead == 0) {
    float t_read = dht.readTemperature();
    float h_read = dht.readHumidity();
    lastRead = millis();

    if (!isnan(t_read) && !isnan(h_read)) {
      temp = t_read;
      hum = h_read;
      logs[logIdx].t = temp;
      logs[logIdx].h = hum;
      logIdx = (logIdx + 1) % 60;
      if (logCount < 60)
        logCount++;

      saveToFS(temp, hum);

      display.fillRect(0, 48, 128, 16, BLACK);
      display.setCursor(0, 48);
      display.printf("%.1fC | %.1f%%", temp, hum);
      display.display();
    }
  }
}
