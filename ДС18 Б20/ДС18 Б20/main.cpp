#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Wi-Fi
const char* ssid = "GalaxyM";
const char* password = "www111www";

#define DS18B20_PIN 4
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress sensorAddr;

WebServer server(80);

#define MAX_HISTORY 20
float tempHistory[MAX_HISTORY];
unsigned long timeHistory[MAX_HISTORY];
int historyIndex = 0;
bool historyFull = false;

void addTempToHistory(float temp) {
  tempHistory[historyIndex] = temp;
  timeHistory[historyIndex] = millis() / 1000;
  historyIndex++;
  if (historyIndex >= MAX_HISTORY) {
    historyIndex = 0;
    historyFull = true;
  }
}

String addrToString(DeviceAddress addr) {
  String str = "";
  for (uint8_t i = 0; i < 8; i++) {
    if (addr[i] < 0x10) str += "0";
    str += String(addr[i], HEX);
    if (i < 7) str += ":";
  }
  return str;
}

String buildWebPage() {
  sensors.begin();
  float currentTemp = DEVICE_DISCONNECTED_C;
  String addrStr = "—";
  bool sensorFound = false;

  if (sensors.getDeviceCount() > 0 && sensors.getAddress(sensorAddr, 0)) {
    sensors.requestTemperatures();
    currentTemp = sensors.getTempC(sensorAddr);
    addrStr = addrToString(sensorAddr);
    sensorFound = true;
    if (currentTemp != DEVICE_DISCONNECTED_C) {
      addTempToHistory(currentTemp);
    }
  }

  String tempStr = (currentTemp != DEVICE_DISCONNECTED_C) ? String(currentTemp, 2) + " °C" : "Ошибка";
  uint32_t uptimeSec = millis() / 1000;

  if (sensorFound && currentTemp != DEVICE_DISCONNECTED_C) {
    // Подготавливаем данные как JS-массив
    String jsData = "[";
    int count = historyFull ? MAX_HISTORY : historyIndex;
    int start = historyFull ? historyIndex : 0;
    for (int i = 0; i < count; i++) {
      if (i > 0) jsData += ",";
      int idx = (start + i) % MAX_HISTORY;
      jsData += String(tempHistory[idx], 1);
    }
    jsData += "]";

    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <meta http-equiv="refresh" content="10">
  <title>Температура</title>
  <style>
    * { box-sizing: border-box; }
    body { 
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; 
      margin: 0; 
      padding: 10px; 
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      -webkit-tap-highlight-color: transparent;
    }
    .container {
      max-width: 500px;
      margin: 0 auto;
    }
    h2 { 
      text-align: center; 
      color: white; 
      margin: 15px 0; 
      font-size: 24px;
      text-shadow: 0 2px 4px rgba(0,0,0,0.3);
    }
    .card { 
      background: rgba(255,255,255,0.95); 
      padding: 20px; 
      border-radius: 16px; 
      box-shadow: 0 8px 32px rgba(0,0,0,0.1); 
      margin: 10px 0;
      backdrop-filter: blur(10px);
      border: 1px solid rgba(255,255,255,0.2);
    }
    .temp { 
      font-size: 32px; 
      font-weight: bold; 
      color: #e74c3c; 
      text-align: center; 
      margin: 15px 0; 
    }
    .addr { 
      font-family: 'Courier New', monospace; 
      font-size: 12px; 
      background: #f8f9fa; 
      padding: 8px; 
      border-radius: 8px; 
      margin: 10px 0; 
      word-break: break-all; 
      text-align: center;
      border: 1px solid #e9ecef;
    }
    .info-line {
      display: flex;
      justify-content: space-between;
      margin: 8px 0;
      font-size: 14px;
      color: #6c757d;
    }
    .graph-container {
      position: relative;
      width: 100%;
      height: 200px;
      margin: 15px 0;
    }
    canvas { 
      width: 100% !important; 
      height: 100% !important;
      border-radius: 12px;
      background: #fff;
      box-shadow: 0 2px 8px rgba(0,0,0,0.1);
    }
    .status {
      text-align: center;
      color: #28a745;
      font-weight: 500;
      margin: 10px 0;
    }
    @media (max-width: 480px) {
      body { padding: 8px; }
      .card { padding: 15px; }
      .temp { font-size: 28px; }
      .graph-container { height: 180px; }
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>🌡️ Монитор температуры</h2>
    <div class="card">
      <div class="info-line">
        <span>Датчик:</span>
        <span>DS18B20</span>
      </div>
      <div class="addr">)rawliteral" + addrStr + R"rawliteral(</div>
      <div class="temp">)rawliteral" + tempStr + R"rawliteral(</div>
      <div class="status">✓ Данные обновляются каждые 10 сек</div>
    </div>
    
    <div class="card">
      <div style="text-align: center; margin-bottom: 10px; font-weight: 500; color: #2c3e50;">
        История измерений
      </div>
      <div class="graph-container">
        <canvas id="tempChart"></canvas>
      </div>
    </div>
  </div>

  <script>
    function initChart() {
      const canvas = document.getElementById('tempChart');
      const ctx = canvas.getContext('2d');
      
      // Устанавливаем правильные размеры canvas
      const container = canvas.parentElement;
      canvas.width = container.clientWidth;
      canvas.height = container.clientHeight;
      
      const data = )rawliteral" + jsData + R"rawliteral(;
      
      if (data.length === 0) {
        drawNoData(ctx, canvas.width, canvas.height);
        return;
      }
      
      drawChart(ctx, canvas.width, canvas.height, data);
    }
    
    function drawNoData(ctx, width, height) {
      ctx.fillStyle = '#f8f9fa';
      ctx.fillRect(0, 0, width, height);
      
      ctx.fillStyle = '#6c757d';
      ctx.font = '14px Arial';
      ctx.textAlign = 'center';
      ctx.fillText('Нет данных для графика', width / 2, height / 2);
    }
    
    function drawChart(ctx, width, height, data) {
      // Очищаем canvas
      ctx.clearRect(0, 0, width, height);
      
      // Настройки
      const padding = 25;
      const chartWidth = width - padding * 2;
      const chartHeight = height - padding * 2;
      
      // Рассчитываем min/max с запасом
      const minTemp = Math.min(...data);
      const maxTemp = Math.max(...data);
      const tempRange = maxTemp - minTemp;
      const paddingValue = tempRange * 0.1; // 10% запас
      
      const yMin = minTemp - paddingValue;
      const yMax = maxTemp + paddingValue;
      const yRange = yMax - yMin;
      
      // Рисуем сетку
      ctx.strokeStyle = '#e9ecef';
      ctx.lineWidth = 1;
      ctx.setLineDash([5, 3]);
      
      // Горизонтальные линии
      for (let i = 0; i <= 4; i++) {
        const y = padding + (chartHeight * (4 - i) / 4);
        ctx.beginPath();
        ctx.moveTo(padding, y);
        ctx.lineTo(width - padding, y);
        ctx.stroke();
        
        // Подписи значений
        ctx.setLineDash([]);
        ctx.fillStyle = '#6c757d';
        ctx.font = '10px Arial';
        ctx.textAlign = 'right';
        const value = yMin + (yRange * i / 4);
        ctx.fillText(value.toFixed(1) + '°C', padding - 5, y + 3);
      }
      
      ctx.setLineDash([]);
      
      // Рисуем линию графика
      ctx.strokeStyle = '#e74c3c';
      ctx.lineWidth = 3;
      ctx.lineJoin = 'round';
      ctx.beginPath();
      
      for (let i = 0; i < data.length; i++) {
        const x = padding + (i / (data.length - 1)) * chartWidth;
        const y = padding + chartHeight - ((data[i] - yMin) / yRange) * chartHeight;
        
        if (i === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      }
      ctx.stroke();
      
      // Рисуем точки
      ctx.fillStyle = '#e74c3c';
      for (let i = 0; i < data.length; i++) {
        const x = padding + (i / (data.length - 1)) * chartWidth;
        const y = padding + chartHeight - ((data[i] - yMin) / yRange) * chartHeight;
        
        ctx.beginPath();
        ctx.arc(x, y, 4, 0, Math.PI * 2);
        ctx.fill();
        
        // Белая обводка точек
        ctx.strokeStyle = 'white';
        ctx.lineWidth = 2;
        ctx.stroke();
        ctx.strokeStyle = '#e74c3c';
      }
      
      // Подписи времени (упрощенные)
      if (data.length > 1) {
        ctx.fillStyle = '#6c757d';
        ctx.font = '10px Arial';
        ctx.textAlign = 'center';
        
        ctx.fillText('начало', padding, height - 5);
        ctx.fillText('сейчас', width - padding, height - 5);
      }
    }
    
    // Инициализация при загрузке
    document.addEventListener('DOMContentLoaded', initChart);
    
    // Перерисовка при изменении размера окна
    let resizeTimer;
    window.addEventListener('resize', function() {
      clearTimeout(resizeTimer);
      resizeTimer = setTimeout(initChart, 250);
    });
  </script>
</body>
</html>
)rawliteral";
  } else {
    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta http-equiv="refresh" content="5">
  <title>DS18B20 Не найден</title>
  <style>
    body { 
      font-family: -apple-system, BlinkMacSystemFont, sans-serif; 
      padding: 20px; 
      background: linear-gradient(135deg, #ff6b6b 0%, #ee5a24 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      margin: 0;
    }
    .error-card {
      background: white;
      padding: 30px;
      border-radius: 20px;
      box-shadow: 0 20px 40px rgba(0,0,0,0.1);
      max-width: 400px;
      text-align: center;
    }
    h2 { 
      color: #c0392b; 
      margin-bottom: 20px;
    }
    .connection-guide {
      text-align: left;
      background: #fff9f9;
      padding: 15px;
      border-radius: 10px;
      margin: 20px 0;
      border-left: 4px solid #e74c3c;
    }
    .connection-guide p {
      margin: 8px 0;
      font-size: 14px;
    }
  </style>
</head>
<body>
  <div class="error-card">
    <h2>❌ Датчик не найден</h2>
    <div class="connection-guide">
      <p><strong>Проверьте подключение:</strong></p>
      <p>• Data → GPIO4</p>
      <p>• VDD → 3.3V</p>
      <p>• GND → GND</p>
      <p>• Резистор 4.7 кОм между Data и 3.3V</p>
    </div>
    <p style="color: #666; font-size: 14px;">Страница обновится автоматически</p>
  </div>
</body>
</html>
)rawliteral";
  }
}

void handleRoot() {
  server.send(200, "text/html", buildWebPage());
}

void setup() {
  Serial.begin(115200);
  
  // Инициализация датчика
  sensors.begin();
  
  // Настройка Wi-Fi
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(IPAddress(10, 0, 0, 1), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0));
  
  // Настройка сервера
  server.on("/", handleRoot);
  server.begin();
  
  Serial.println("Готово!");
  Serial.println("Wi-Fi: GalaxyM / www111www");
  Serial.println("Откройте: http://10.0.0.1");
  Serial.println("График адаптирован для мобильных устройств!");
}

void loop() {
  server.handleClient();
  delay(100);
}