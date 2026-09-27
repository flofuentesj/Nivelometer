#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <MPU9250.h>

TFT_eSPI tft = TFT_eSPI();
MPU9250 mpu;

// Paleta de colores RGB565 (Fondo rosa claro y tonos oscuros)
#define LIGHT_PINK 0xFDB8   // Fondo rosa claro suave
#define DARK_PURPLE 0x780F  // Eje X (Morado oscuro)
#define DARK_GREEN 0x0320   // Eje Y (Verde oscuro)
#define DARK_CYAN 0x02CE    // Eje Z (Celeste / Azul oscuro)
#define DARK_PINK 0xC010    // Burbuja central en rosa profundo
#define RETICLE_GRAY 0x8410 // Retículo de referencia
#define TEXT_DARK 0x3186    // Texto oscuro para alto contraste

// Variables de seguimiento para borrado selectivo sin parpadeo
int prev_endX_x = 120, prev_endX_y = 120;
int prev_endY_x = 120, prev_endY_y = 120;
int prev_endZ_x = 120, prev_endZ_y = 120;
int prev_bubbleX = 120, prev_bubbleY = 120;
char prev_textX[16] = "";
char prev_textY[16] = "";
char prev_textZ[16] = "";
char prev_pitchRoll[32] = "";

void setup()
{
  Serial.begin(115200);
  Wire.begin(21, 22);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(LIGHT_PINK);

  if (!mpu.setup(0x68))
  {
    Serial.println("MPU9250 no encontrado en 0x68!");
    tft.setTextColor(TEXT_DARK, LIGHT_PINK);
    tft.setCursor(40, 110);
    tft.print("Error MPU9250");
    while (1)
      ;
  }

  tft.setTextDatum(MC_DATUM);

  // Dibujar retículo óptico estático de referencia una sola vez
  int cx = 120, cy = 120, maxRadius = 95;
  tft.drawCircle(cx, cy, maxRadius, RETICLE_GRAY);
  tft.drawCircle(cx, cy, maxRadius / 2, 0xBDF7);
  tft.drawCircle(cx, cy, 4, TEXT_DARK);
  tft.drawFastHLine(cx - maxRadius, cy, maxRadius * 2, 0xBDF7);
  tft.drawFastVLine(cx, cy - maxRadius, maxRadius * 2, 0xBDF7);
}

void loop()
{
  if (mpu.update())
  {
    float ax = mpu.getAccX();
    float ay = mpu.getAccY();
    float az = mpu.getAccZ();

    int cx = 120;
    int cy = 120;
    int maxRadius = 95;

    // Calcular ángulos de inclinación óptica (Pitch y Roll en grados)
    float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
    float roll = atan2(ay, az) * 180.0 / PI;

    // --- 1. BORRAR ELEMENTOS ANTERIORES PINTÁNDOLOS DEL COLOR DE FONDO ---
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

    // Restaurar el cero central de referencia
    tft.drawCircle(cx, cy, 4, TEXT_DARK);

    // --- 2. CALCULAR NUEVAS POSICIONES ---
    int axisLength = 55;

    int endX_x = cx + (int)(axisLength * cos(roll * PI / 180.0));
    int endX_y = cy + (int)(axisLength * sin(pitch * PI / 180.0));

    int endY_x = cx - (int)(axisLength * sin(roll * PI / 180.0));
    int endY_y = cy + (int)(axisLength * cos(pitch * PI / 180.0));

    int endZ_x = cx - (int)(axisLength * 0.4 * ax);
    int endZ_y = cy - (int)(axisLength * 0.4 * ay);

    // Burbuja de precisión
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

    // --- 3. DIBUJAR NUEVOS ELEMENTOS Y SUS VALORES AL LADO DE LA LÍNEA ---
    // Eje X (Morado oscuro)
    tft.drawLine(cx, cy, endX_x, endX_y, DARK_PURPLE);
    tft.setTextColor(DARK_PURPLE, LIGHT_PINK);
    sprintf(prev_textX, "X:%.2f", ax);
    tft.drawString(prev_textX, endX_x + 12, endX_y, 2);

    // Eje Y (Verde oscuro)
    tft.drawLine(cx, cy, endY_x, endY_y, DARK_GREEN);
    tft.setTextColor(DARK_GREEN, LIGHT_PINK);
    sprintf(prev_textY, "Y:%.2f", ay);
    tft.drawString(prev_textY, endY_x, endY_y + 12, 2);

    // Eje Z (Celeste / Azul oscuro)
    tft.drawLine(cx, cy, endZ_x, endZ_y, DARK_CYAN);
    tft.setTextColor(DARK_CYAN, LIGHT_PINK);
    sprintf(prev_textZ, "Z:%.2f", az);
    tft.drawString(prev_textZ, endZ_x - 12, endZ_y - 12, 2);

    // Burbuja de alineación central
    tft.fillCircle(bubbleX, bubbleY, 8, DARK_PINK);
    tft.drawCircle(bubbleX, bubbleY, 8, TEXT_DARK);

    // Panel superior de ángulos generales (Pitch y Roll)
    tft.setTextColor(TEXT_DARK, LIGHT_PINK);
    sprintf(prev_pitchRoll, "P:%.1f R:%.1f", pitch, roll);
    tft.drawString(prev_pitchRoll, cx, 18, 2);

    // Actualizar registros previos
    prev_endX_x = endX_x;
    prev_endX_y = endX_y;
    prev_endY_x = endY_x;
    prev_endY_y = endY_y;
    prev_endZ_x = endZ_x;
    prev_endZ_y = endZ_y;
    prev_bubbleX = bubbleX;
    prev_bubbleY = bubbleY;

    delay(30);
  }
}