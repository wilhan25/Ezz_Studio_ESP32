#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <ui/ui.h>
#include <ui/screens.h>

TFT_eSPI display = TFT_eSPI();

#define SCREEN_HEIGHT 128
#define SCREEN_WIDTH  160

// Config do sensor de distância
#define TRIG  14
#define ECHO  13
#define MAX_DISTANCE 700
float timeOut = MAX_DISTANCE * 60; 
int soundVelocity = 340;

// Criação do Semáforo (Mutex) para proteger o LVGL
SemaphoreHandle_t xGuiMutex;

// Config, funções e variáveis do lvgl
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 10];

void tarefaLVGL(void *pvParameters) {
  while (1) {
    // Tenta pegar a chave do LVGL. Se conseguir, processa a tela.
    if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE) {
      lv_timer_handler(); 
      xSemaphoreGive(xGuiMutex); // Devolve a chave
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void tarefaTick(void *pvParameters) {
  while (1) {
    lv_tick_inc(1);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void meu_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  display.startWrite();
  display.setAddrWindow(area->x1, area->y1, w, h);
  display.pushColors((uint16_t *)&color_p->full, w * h, true);
  display.endWrite();
  lv_disp_flush_ready(disp);
}

void TaskDistancia(void *pv){
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  unsigned long pingTime;
  float distancia;

  lv_color_t azul = lv_palette_main(LV_PALETTE_BLUE);
  lv_color_t vermelho = lv_palette_main(LV_PALETTE_RED);
  
  while (1) {
    digitalWrite(TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG, LOW);
    
    // Medição do tempo de resposta
    pingTime = pulseIn(ECHO, HIGH, timeOut);
    distancia = (float)pingTime * soundVelocity / 2 / 10000;

    Serial.printf("Distance: ");
    Serial.println(distancia);

    uint8_t calculo_cor = (uint8_t)((distancia/150)*255);
    lv_color_t corBarra = lv_color_mix(vermelho, azul, calculo_cor);

    // Tenta pegar a chave do LVGL para atualizar os componentes com segurança
    if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE) {
      lv_bar_set_range(objects.barra_dist, 0, 150);
      lv_bar_set_value(objects.barra_dist, (int)distancia, LV_ANIM_ON);
      lv_obj_set_style_bg_color(objects.barra_dist,corBarra, LV_PART_INDICATOR| LV_STATE_DEFAULT);
      lv_label_set_text(objects.valor_dist, String(distancia, 2).c_str());
      
      xSemaphoreGive(xGuiMutex); // Devolve a chave do LVGL
    }

    // Tempo de respiro essencial para o ultrassom e para o FreeRTOS
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void setup() {
  Serial.begin(115200);
  display.init();
  display.setRotation(3);

  // Inicializa o Mutex ANTES de criar as tarefas
  xGuiMutex = xSemaphoreCreateMutex();

  // Inicialização do LVGL
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 10);
  static lv_disp_drv_t driver_display;
  lv_disp_drv_init(&driver_display); 
  driver_display.hor_res = SCREEN_WIDTH;
  driver_display.ver_res = SCREEN_HEIGHT;
  driver_display.flush_cb = meu_disp_flush;
  driver_display.draw_buf = &draw_buf;
  lv_disp_drv_register(&driver_display);
  ui_init();

  // Cria as tarefas no FreeRTOS
  xTaskCreate(tarefaTick, "Tick", 1024, NULL, 3, NULL);
  xTaskCreate(tarefaLVGL, "Time_lvgl", 4096, NULL, 2, NULL);
  xTaskCreate(TaskDistancia, "distancia", 4096, NULL, 4, NULL);
}

void loop() {
}