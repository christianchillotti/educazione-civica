
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#define RED_LED 18
#define GREEN_LED 17
#define BLUE_LED 16

#define THRESHOLD_X 0.25
#define THRESHOLD_Y 0.25
#define THRESHOLD_Z 0.25

#define SENS_SCALE_0 16384.0 // +-1g
#define SENS_SCALE_1 8192.0  // +-2g
#define SENS_SCALE_2 4096.0  // +-4g
#define SENS_SCALE_3 2048.0  // +-8g

#define MPU_SDA 21
#define MPU_SCL 22
#define LCD_SDA 23
#define LCD_SCL 19

#define MPU_ADDRESS 0x68
#define LCD_ADDRESS 0x27

TwoWire mpu_i2c = TwoWire(1);

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

float x_0, y_0, z_0;
bool first = true;

const char* ssid = "";
const char* password = "";

void check_thresholds(float x, float y, float z)
{
  // Check of thresholds
  if (abs(x) > THRESHOLD_X)
    digitalWrite(RED_LED, HIGH);
  else 
    digitalWrite(RED_LED, LOW);
  if (abs(y) > THRESHOLD_Y) 
    digitalWrite(BLUE_LED, HIGH);
  else
    digitalWrite(BLUE_LED, LOW);
  if (abs(z) > THRESHOLD_Z) 
    digitalWrite(GREEN_LED, HIGH);
  else
    digitalWrite(GREEN_LED, LOW);
}

void init_LED(){
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
}

void connect_to_wifi() {
  Serial.println("Connessione alla rete wifi \"" + String(ssid) + "\" in corso");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.print("\nConnessione alla rete wifi \"" + String(ssid) + "\" completata: ");
  Serial.println(WiFi.localIP());  // IP Address

  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
}

void init_mpu() {
  Serial.println("Inizializzazione dell'accelerometro in corso...");
  mpu_i2c.begin(MPU_SDA, MPU_SCL);            // Wire library initialization
  mpu_i2c.beginTransmission(MPU_ADDRESS);     // Begin transmission to MPU
  mpu_i2c.write(0x6B);                        // PWR_MGMT_1 register
  mpu_i2c.write(0);                           // MPU-6050 to start mode
  mpu_i2c.endTransmission(true);
  Serial.println("Inizializzazione dell'accelerometro completata.");
}

void init_lcd() {
  Serial.println("\nInizializzazione del display LCD in corso...");
  Wire.begin(LCD_SDA, LCD_SCL); // Inizializza I2C con pin personalizzati
  delay(100);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("IP address:");
  Serial.println("Inizializzazione del display LCD completata.");
}

WebServer server;
WebSocketsServer webSocket = WebSocketsServer(81);

const char webpage[] PROGMEM = R"rawliteral(
  <html>
    <script src='https://cdn.plot.ly/plotly-latest.min.js'></script>
    <head>
      <title> Sismografo 2.0 </title>
      <script>
        var Socket;
        function init() {
          Socket = new WebSocket('ws://' + window.location.hostname + ':81/');

          Plotly.newPlot('plot', 
            [
              {
                x: [],
                y: [],
                mode: 'lines',
                name: 'Asse X',
                line: {color: '#FF0000'}
              }, 
              {
                x: [],
                y: [],
                mode: 'lines',
                name: 'Asse Y',
                line: {color: '#0000FF'}
              },
              {
                x: [],
                y: [],
                mode: 'lines',
                name: 'Asse Z ',
                line: {color: '#00FF00'}
              }
            ]
          );

          var cnt = 0;

          Socket.onmessage = function(event) {
            var point = event.data.split(','); // array of 3 cells [x,y,z]
            console.log("x = ", point[0], ", y = ", point[1], ", z = ", point[2]);

            var options = { weekday: 'long', year: 'numeric', month: 'long', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric' };
            var now = new Date();

            var interval = setInterval(function() {
                var time = new Date();

                var update = {
                  x: [[time], [time], [time]],
                  y: [[point[0]], [point[1]], [point[2]]]
                }

                var older_time = time.setMinutes(time.getMinutes() - 1);
                var future_time = time.setMinutes(time.getMinutes() + 1);

                var minute_view = {
                  xaxis: {
                    type: 'date',
                    range: [older_time, future_time]
                  }
                };

                Plotly.extendTraces('plot', update, [0, 1, 2]);
                Plotly.relayout('plot', minute_view);                
                clearInterval(interval);
            }, 1000);

            var axises = "";
            var msg = "";
            var n_axises = 0;
           
            if (Math.abs(point[0]) > 0.25) {
                axises = axises + " x (" + point[0] + " g)";
                n_axises++;
            }
            if (Math.abs(point[1]) > 0.25) {
                axises = axises + " y (" + point[1] + " g)"
                n_axises++;
            }
            if (Math.abs(point[2]) > 0.25) {
                axises = axises + " z (" + point[2] + " g)"
                n_axises++;
            }
            if (n_axises > 0) {
                msg = "[" + now.toLocaleDateString("it-IT", options) + "]" + " superata soglia di accelerazione ";
                if (n_axises > 1)
                  msg = msg + "sugli assi" + axises;
                else 
                  msg = msg + "sull'asse" + axises;

                // open new widown in which there is popup alert
                var window_alert = window.open("", "Sismografo 2.0 alert", "width=600,height=100");
                window_alert.document.write("<title>Sismografo 2.0 alert<\/title>");
                window_alert.document.write(msg+"<br>");
            }
          }
        }
      </script>
    </head>
    <body onload="init()">
      <h1> Sismografo 2.0 con ESP32 </h1>
      <div id="plot" style="width:100%;height:100%;"></div>
    </body>
  </html>
)rawliteral";

void init_web_server()
{
  server.on("/",[](){
    server.send_P(200, "text/html", webpage);  
  });

  // Start the server
  server.begin();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT)  
    Serial.printf("payload [%u]: %s\n", num, payload);
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  init_LED();
  init_mpu();
  init_lcd();
  connect_to_wifi();
  init_web_server();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
 // init_led();
}

void loop() {
  webSocket.loop();
  server.handleClient();
  
  int x_raw = 0; // read from sensor
  int y_raw = 0; // read from sensor
  int z_raw = 0; // read from sensor

  mpu_i2c.beginTransmission(MPU_ADDRESS);      // Start transfer
  mpu_i2c.write(0x3B);                         // register 0x3B (ACCEL_XOUT_H), records data in queue
  mpu_i2c.endTransmission(false);              // Maintain connection
  mpu_i2c.requestFrom(MPU_ADDRESS, 14, true);  // Request data to MPU

  //Reads byte by byte
  x_raw = mpu_i2c.read() << 8 | mpu_i2c.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  y_raw = mpu_i2c.read() << 8 | mpu_i2c.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  z_raw = mpu_i2c.read() << 8 | mpu_i2c.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)

  String raw = "x_raw: " + String(x_raw) + ", y_raw: " + String(y_raw) + ", z_raw: " + String(z_raw);
  Serial.println(raw);

  float x = (float) x_raw / SENS_SCALE_0;
  float y = (float) y_raw / SENS_SCALE_0;
  float z = (float) z_raw / SENS_SCALE_0;

  if (first) {
    x_0 = x;
    y_0 = y;
    z_0 = z;
    first = false;
  } else {
    check_thresholds(x-x_0, y-y_0, z-z_0); 
  }

  // Send data
  String point = String(x-x_0) + String(",") + String(y-y_0) + String(",") + String(z-z_0);
  webSocket.broadcastTXT(point);
  delay(250);
}
