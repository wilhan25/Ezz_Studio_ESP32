#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <ui/ui.h>
#include <ui/screens.h>

TFT_eSPI display = TFT_eSPI();

#define SCREEN_HEIGHT 128
#define SCREEN_WIDTH  160

// joystick
int xPin = 13, yPin = 12;

// Config, funções e variáveis do lvgl
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 10];
lv_group_t * grupoBTN;

// CRIAÇÃO DO MUTEX PARA PROTEGER O LVGL
SemaphoreHandle_t xGuiMutex;

void tarefaLVGL(void *pvParameters) {
  while (1) {
    // Só mexe no LVGL se a chave (Mutex) estiver livre
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

void TaskMenu(void *pv){
  // CORREÇÃO: Pinos analógicos devem ser apenas INPUT
  pinMode(xPin, INPUT);
  pinMode(yPin, INPUT);
  
  lv_obj_t* grid[2][2] = {
    {objects.btn1, objects.btn2},
    {objects.btn3, objects.btn4}
  };
  
  int linha = 0, coluna = 0;
  
  while (1) {
    int xVal = analogRead(xPin);
    int yVal = analogRead(yPin);
    bool muda_foco = false;

    // Lógica de movimentação por eixos
    if(xVal > 3000 && coluna < 1){       // Direita
      coluna++;
      muda_foco = true;      
    }
    else if(xVal < 1000 && coluna > 0){  // Esquerda
      coluna--;
      muda_foco = true;
    }
    else if(yVal > 3000 && linha < 1){   // Baixo (Aumenta a linha)
      linha++;
      muda_foco = true;
    }
    else if(yVal < 1000 && linha > 0){   // Cima (Diminui a linha)
      linha--;
      muda_foco = true;
    }

    // Se mudou a posição, aplica o foco protegendo com o Mutex
    if (muda_foco) {
      if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE) {
        lv_group_focus_obj(grid[linha][coluna]);
        xSemaphoreGive(xGuiMutex);
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(muda_foco ? 250 : 50));
  }  
}

void setup() {
  Serial.begin(115200);
  
  // CRIALÇÃO MANDATÓRIA DO MUTEX BEFORE AS TASKS
  xGuiMutex = xSemaphoreCreateMutex();

  display.init();
  display.setRotation(3);

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

  ui_init();

  // ADICIONE ESTA LINHA PARA LIMPAR O FOCO TRAVADO DO SQUARELINE:
  lv_obj_clear_state(objects.btn1, LV_STATE_FOCUSED);

  // Grupo para os botões gerenciarem os focos
  grupoBTN = lv_group_create();
  lv_group_add_obj(grupoBTN, objects.btn1);
  lv_group_add_obj(grupoBTN, objects.btn2);
  lv_group_add_obj(grupoBTN, objects.btn3);
  lv_group_add_obj(grupoBTN, objects.btn4);

  if (xSemaphoreTake(xGuiMutex, portMAX_DELAY) == pdTRUE) {
    lv_group_focus_obj(objects.btn1);
    xSemaphoreGive(xGuiMutex);
  }

  // Cria as tarefas no FreeRTOS
  xTaskCreate(tarefaTick, "Tick", 1024, NULL, 3, NULL);
  xTaskCreate(tarefaLVGL, "Time_lvgl", 4096, NULL, 2, NULL);
  xTaskCreate(TaskMenu, "menu", 3072, NULL, 2, NULL);
}

void loop() {
  // Mantido vazio - FreeRTOS assume o controle
}