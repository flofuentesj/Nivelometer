#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

TFT_eSPI tft = TFT_eSPI();
Adafruit_MPU6050 mpu;

// Configuración del Botón
const int BUTTON_PIN = 13;
bool lastButtonState = HIGH;
int currentView = 0; // 0: Retículo 3D, 1: Vista ampliada de Pitch/Roll
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Paleta de colores RGB565 (Fondo rosa claro y tonos oscuros)
#define LIGHT_PINK 0xFDB8   // Fondo rosa claro suave
#define DARK_PURPLE 0x780F  // Eje X (Morado oscuro)
#define DARK_GREEN 0x0320   // Eje Y (Verde oscuro)
#define DARK_CYAN 0x02CE    // Eje Z (Celeste / Azul oscuro)
#define DARK_PINK 0xC010    // Burbuja central en rosa profundo
#define RETICLE_GRAY 0x8410 // Retículo de referencia
#define TEXT_DARK 0x3186    // Texto oscuro para alto contraste

// Variables de seguimiento para borrado selectivo en Vista 0
int prev_endX_x = 120, prev_endX_y = 120;
int prev_endY_x = 120, prev_endY_y = 120;
int prev_endZ_x = 120, prev_endZ_y = 120;
int prev_bubbleX = 120, prev_bubbleY = 120;
char prev_textX[16] = "";
char prev_textY[16] = "";
char prev_textZ[16] = "";
char prev_pitchRoll[32] = "";

// Variables para Vista 1 (Pitch y Roll ampliado)
float old_pitch = -999.0;
float old_roll = -999.0;

void drawStaticReticle()
{
  int cx = 120, cy = 120, maxRadius = 95;
  tft.drawCircle(cx, cy, maxRadius, RETICLE_GRAY);
  tft.drawCircle(cx, cy, maxRadius / 2, 0xBDF7);
  tft.drawCircle(cx, cy, 4, TEXT_DARK);
  tft.drawFastHLine(cx - maxRadius, cy, maxRadius * 2, 0xBDF7);
  tft.drawFastVLine(cx, cy - maxRadius, maxRadius * 2, 0xBDF7);
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(21, 22);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(LIGHT_PINK);

  if (!mpu.begin())
  {
    Serial.println("MPU6050 no encontrado!");
    tft.setTextColor(TEXT_DARK, LIGHT_PINK);
    tft.setCursor(40, 110);
    tft.print("Error MPU6050");
    while (1)
      ;
  }

  // Configuración del sensor MPU6050
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  tft.setTextDatum(MC_DATUM);
  drawStaticReticle();
}

void loop()
{
  // --- LECTURA DEL BOTÓN CON ANTIRREBOTE (DEBOUNCE) ---
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState)
  {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    if (reading == LOW && lastButtonState == HIGH)
    {
      currentView = (currentView + 1) % 2; // Alternar vista 0 y 1
      tft.fillScreen(LIGHT_PINK);          // Limpiar pantalla al cambiar de vista
      if (currentView == 0)
      {
        drawStaticReticle();
      }
      old_pitch = -999.0;
      old_roll = -999.0;
    }
  }
  lastButtonState = reading;

  // --- LECTURA DE DATOS DEL MPU6050 ---
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Normalizar aceleración a unidades de 'g'
  float ax = a.acceleration.x / 9.80665;
  float ay = a.acceleration.y / 9.80665;
  float az = a.acceleration.z / 9.80665;

  float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
  float roll = atan2(ay, az) * 180.0 / PI;

  // ==========================================
  // VISTA 0: Retículo 3D y Burbuja de Precisión
  // ==========================================
  if (currentView == 0)
  {
    int cx = 120;
    int cy = 120;
    int maxRadius = 95;

    // Borrar elementos anteriores
    tft.drawLine(cx, cy, prev_endX_x, prev_endX_y, LIGHT_PINK);
    tft.drawLine(cx, cy, prev_endY_x, prev_endY_y, LIGHT_PINK);
    tft.drawLine(cx, cy, prev_endZ_x, prev_endZ_y, LIGHT_PINK);
    tft.fillCircle(prev_bubbleX, prev_bubbleY, 8, LIGHT_PINK);

    if (strlen(prev_textX) > 0)
    {
      tft.setTextColor(LIGHT_PINK, LIGHT_PINK);
      tft.drawString(prev_textX, prev_endX_x + 12, prev_endX_y, 2);
      tft.drawString(prev_textY, prev_endY_x, prev_endY_y + 12, 2);
      tft.drawString(prev_textZ, prev_endZ_x - 12, prev_endZ_y - 12, 2);
      tft.drawString(prev_pitchRoll, cx, 18, 2);
    }

    tft.drawCircle(cx, cy, 4, TEXT_DARK);

    // Calcular nuevas posiciones
    int axisLength = 55;
    int endX_x = cx + (int)(axisLength * cos(roll * PI / 180.0));
    int endX_y = cy + (int)(axisLength * sin(pitch * PI / 180.0));

    int endY_x = cx - (int)(axisLength * sin(roll * PI / 180.0));
    int endY_y = cy + (int)(axisLength * cos(pitch * PI / 180.0));

    int endZ_x = cx - (int)(axisLength * 0.4 * ax);
    int endZ_y = cy - (int)(axisLength * 0.4 * ay);

    int bubbleX = cx + (int)(ay * 40.0);
    int bubbleY = cy + (int)(ax * 40.0);

    int dx = bubbleX - cx;
    int dy = bubbleY - cy;
    if (dx * dx + dy * dy > (maxRadius - 5) * (maxRadius - 5))
    {
      float angle = atan2(dy, dx);
      bubbleX = cx + (int)((maxRadius - 5) * cos(angle));
      bubbleY = cy + (int)((maxRadius - 5) * sin(angle));
    }

    // Dibujar nuevos elementos
    tft.drawLine(cx, cy, endX_x, endX_y, DARK_PURPLE);
    tft.setTextColor(DARK_PURPLE, LIGHT_PINK);
    sprintf(prev_textX, "X:%.2f", ax);
    tft.drawString(prev_textX, endX_x + 12, endX_y, 2);

    tft.drawLine(cx, cy, endY_x, endY_y, DARK_GREEN);
    tft.setTextColor(DARK_GREEN, LIGHT_PINK);
    sprintf(prev_textY, "Y:%.2f", ay);
    tft.drawString(prev_textY, endY_x, endY_y + 12, 2);

    tft.drawLine(cx, cy, endZ_x, endZ_y, DARK_CYAN);
    tft.setTextColor(DARK_CYAN, LIGHT_PINK);
    sprintf(prev_textZ, "Z:%.2f", az);
    tft.drawString(prev_textZ, endZ_x - 12, endZ_y - 12, 2);

    tft.fillCircle(bubbleX, bubbleY, 8, DARK_PINK);
    tft.drawCircle(bubbleX, bubbleY, 8, TEXT_DARK);

    tft.setTextColor(TEXT_DARK, LIGHT_PINK);
    sprintf(prev_pitchRoll, "P:%.1f R:%.1f", pitch, roll);
    tft.drawString(prev_pitchRoll, cx, 18, 2);

    prev_endX_x = endX_x;
    prev_endX_y = endX_y;
    prev_endY_x = endY_x;
    prev_endY_y = endY_y;
    prev_endZ_x = endZ_x;
    prev_endZ_y = endZ_y;
    prev_bubbleX = bubbleX;
    prev_bubbleY = bubbleY;
  }
  // ==========================================
  // VISTA 1: Panel Ampliado de Pitch y Roll
  // ==========================================
  else if (currentView == 1)
  {
    int cx = 120, cy = 120;

    // Título de la vista
    tft.setTextColor(TEXT_DARK, LIGHT_PINK);
    tft.drawString("--- VISTA ANGULAR ---", cx, 35, 2);

    char buf[32];

    // Dibujar etiquetas fijas una sola vez o con actualización limpia
    tft.drawString("PITCH (Cabeceo):", cx, 75, 2);
    sprintf(buf, "%+.2f deg", pitch);
    tft.setTextColor(DARK_PURPLE, LIGHT_PINK);
    tft.setTextPadding(140);
    tft.drawString(buf, cx, 105, 4);

    tft.setTextColor(TEXT_DARK, LIGHT_PINK);
    tft.drawString("ROLL (Alabeo):", cx, 150, 2);
    sprintf(buf, "%+.2f deg", roll);
    tft.setTextColor(DARK_GREEN, LIGHT_PINK);
    tft.setTextPadding(140);
    tft.drawString(buf, cx, 180, 4);
  }

  delay(30);
}