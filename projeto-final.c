/*
 *
 *    Projeto: Plataforma de Sensores de Etileno, Temperatura, Umidade e CO₂
 *    Periféricos: Display OLED (SSD1306 via I2C), 2 potenciômetros (simulam sensores),
 *                 3 botões: BUTTON_NEXT (pino 5), BUTTON_BACK (pino 6) e BUTTON_SET (pino 22),
 *                 2 buzzers e LED RGB (pinos 13 - R, 12 - G, 11 - B).
 *    Objetivo: Exibir os valores medidos e, através do botão SET, permitir que o usuário ajuste os
 *              setpoints para definir as faixas de classificação de cada sensor.
 *
 *    Comportamento:
 *      - No modo normal, o OLED mostra a leitura e a classificação; a matriz WS2812 exibe uma carinha
 *        (triste, neutra ou feliz) de acordo com o nível medido.
 *      - No modo de configuração (quando BUTTON_SET é pressionado), o sistema exibe o dígito atual
 *        (padrão digital) na matriz.
 *      - Os botões NEXT e BACK aumentam ou diminuem o setpoint do sensor ativo.
 *      - Para temperatura e umidade, o LED indicador (R_LED_PIN) gera um sinal PWM que simula a velocidade
 *        de um motor (por exemplo, para ajustar a refrigeração). Agora, se a temperatura estiver acima do
 *        setpoint superior (temp_upper), o PWM é aplicado na saída azul (B_LED_PIN), indicando a ação do motor
 *        de uma máquina de resfriamento.
 *      - Novo modo (menu 4): exibe os valores médios de cada sensor desde o início e o tempo decorrido.
 *
 *    Tela inicial: Exibe "FruitLife Device" no OLED e toca uma musiquinha via buzzer.
 *
 *    Adaptado e expandido por Heitor Lemos TIC370100366
 */

 #include "pico/stdlib.h"
 #include "hardware/i2c.h"
 #include "hardware/pwm.h"
 #include "hardware/adc.h"
 #include "include/ssd1306.h"    // OLED
 #include "include/font.h"       // Fonte OLED
 #include "ws2812.pio.h"         // WS2812 via PIO
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 
 //=================================================
 // Variáveis globais para modo configuração
 //=================================================
 volatile int digito_atual = 0;           // Para exibir dígito (modo configuração)
 volatile bool atualizar_exibicao = false; // Flag para atualizar a matriz (modo configuração)
 
 //===============================================
 // OLED
 //===============================================
 const uint8_t SDA = 14;
 const uint8_t SCL = 15;
 #define I2C_ADDR 0x3C
 #define SSD1306_WIDTH 128
 #define SSD1306_HEIGHT 64
 
 //===============================================
 // Sensores (simulados por potenciômetros)
 //===============================================
 #define POT_ETILENO_PIN 27    // Simula Gás Etileno e CO₂
 #define POT_UMIDADE_PIN 26    // Simula Temperatura e Umidade
 
 //===============================================
 // Botões
 //===============================================
 #define BUTTON_NEXT 5   // Avança (menu ou aumenta setpoint)
 #define BUTTON_BACK 6   // Retrocede (menu ou diminui setpoint)
 #define BUTTON_SET 22   // Entra/saí do modo de configuração
 
 //===============================================
 // Buzzers
 //===============================================
 #define BUZZER1_PIN 10  // Startup/alerta
 #define BUZZER2_PIN 21  // Alertas
 
 //===============================================
 // LED RGB (PWM)
 //===============================================
 #define R_LED_PIN 13   // Também usado para indicar o "motor" em modos de Temperatura e Umidade
 #define G_LED_PIN 11
 #define B_LED_PIN 12
 #define PWM_WRAP 255
     //===============================================
 // Funções para LED RGB
 //===============================================
 void init_rgb_led() {
    uint8_t pins[3] = {R_LED_PIN, G_LED_PIN, B_LED_PIN};
    for (int i = 0; i < 3; i++) {
        gpio_set_function(pins[i], GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(pins[i]);
        pwm_set_wrap(slice, PWM_WRAP);
        pwm_set_enabled(slice, true);
    }
  }
  
  void set_rgb_color(uint8_t r, uint8_t g, uint8_t b) {
    pwm_set_chan_level(pwm_gpio_to_slice_num(R_LED_PIN), pwm_gpio_to_channel(R_LED_PIN), r);
    pwm_set_chan_level(pwm_gpio_to_slice_num(G_LED_PIN), pwm_gpio_to_channel(G_LED_PIN), g);
    pwm_set_chan_level(pwm_gpio_to_slice_num(B_LED_PIN), pwm_gpio_to_channel(B_LED_PIN), b);
  }
 
 //===============================================
 // Matriz WS2812
 //===============================================
 #define NUM_PIXELS 25
 #define WS2812_PIN 7
 #define IS_RGBW false
 bool buffer_leds[NUM_PIXELS] = { false };
 
 //===============================================
 // Outras definições e variáveis globais
 //===============================================
 #define INTERVALO_PISCA_LED_MS 100  // ms
 const uint32_t DEBOUNCE_DELAY_MS = 200;
 
 // Menu: 0 = Gás Etileno, 1 = Temperatura, 2 = Umidade, 3 = CO₂, 4 = Médias
 volatile int menu_index = 0;
 volatile uint32_t last_button_interrupt_time = 0;
 volatile bool in_set_mode = false;
 volatile int current_set_param = 0; // 0 = setpoint inferior, 1 = setpoint superior
 
 // Setpoints para cada sensor
 volatile float etileno_lower = 3.0f;
 volatile float etileno_upper = 7.0f;
 volatile float temp_lower = 10.0f;
 volatile float temp_upper = 15.0f;
 volatile float umidade_set = 90.0f;
 volatile float co2_set = 800.0f;  // Exemplo
 
 // Cores para a matriz WS2812
 #define COR_WS2812_R 0
 #define COR_WS2812_G 0
 #define COR_WS2812_B 80
 
 //-------------------------------------------------
 // Variáveis para cálculo das médias
 //-------------------------------------------------
 volatile uint32_t sample_count = 0;
 volatile float sum_etileno = 0.0f;
 volatile float sum_temp = 0.0f;
 volatile float sum_umidade = 0.0f;
 volatile float sum_co2 = 0.0f;
 absolute_time_t start_time;
 
 
  
 //===============================================
 // Padrões de carinhas (5x5) – Modo normal
 // Índice 0: Neutra, 1: Feliz, 2: Triste
 //===============================================
 const bool padroes_carinhas[3][5][5] = {
     
     { // Carinha Feliz
       {false, true, true, true, false},
       {false, true, false, true, false},
       {false, false, false, false, false},
       {false, true, false, true, false},
       {false, true, false, true, false}
     },
     { // Carinha Triste
       {false, true, false, true, false},
       {false, true, true, true, false},
       {false, false, false, false, false},
       {false, true, false, true, false},
       {false, true, false, true, false}
    }

    
     
};
   

   
 //===============================================
 // Função para atualizar o buffer com uma carinha (modo normal)
 //===============================================
 void atualizar_buffer_com_carinha(int tipo) {
     // tipo: 0 = neutra, 1 = feliz, 2 = triste
     for (int linha = 0; linha < 5; linha++) {
         for (int coluna = 0; coluna < 5; coluna++) {
             int indice = linha * 5 + coluna;
             buffer_leds[indice] = padroes_carinhas[tipo][linha][coluna];
         }
     }
 }
   
 //===============================================
 // Funções auxiliares para os WS2812
 //===============================================
 static inline void enviar_pixel(uint32_t pixel_grb) {
     pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
 }
   
 static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
     return ((uint32_t)r << 8) | ((uint32_t)g << 16) | (uint32_t)b;
 }
   
 void definir_leds(uint8_t r, uint8_t g, uint8_t b) {
     uint32_t cor = urgb_u32(r, g, b);
     for (int i = 0; i < NUM_PIXELS; i++) {
         if (buffer_leds[i])
             enviar_pixel(cor);
         else
             enviar_pixel(0);
     }
     sleep_us(60);
 }
   
 //===============================================
 // Função para gerar um tom no buzzer (bloqueante)
 //===============================================
 void play_tone(uint gpio, int frequency, int duration_ms) {
     uint slice_num = pwm_gpio_to_slice_num(gpio);
     uint channel = pwm_gpio_to_channel(gpio);
     gpio_set_function(gpio, GPIO_FUNC_PWM);
     float divider = 1.0f;
     uint32_t wrap = (uint32_t)((125000000.0f / (divider * frequency)) - 1);
     pwm_set_clkdiv(slice_num, divider);
     pwm_set_wrap(slice_num, wrap);
     pwm_set_chan_level(slice_num, channel, (wrap + 1) / 2);
     pwm_set_enabled(slice_num, true);
     sleep_ms(duration_ms);
     pwm_set_enabled(slice_num, false);
     gpio_set_function(gpio, GPIO_FUNC_SIO);
     gpio_set_dir(gpio, GPIO_OUT);
     gpio_put(gpio, 0);
 }
   
 //===============================================
 // Função para reproduzir uma musiquinha no startup
 //===============================================
 void play_startup_music() {
     struct {
         int frequency;
         int duration;
     } notes[] = {
         {261, 200}, {293, 200}, {329, 200}, {392, 200}, {329, 200}, {261, 200}
     };
     int note_count = sizeof(notes) / sizeof(notes[0]);
     for (int i = 0; i < note_count; i++) {
          play_tone(BUZZER1_PIN, notes[i].frequency, notes[i].duration);
          sleep_ms(50);
     }
 }
   
 //===============================================
 // Tela inicial (Splash Screen) no OLED
 //===============================================
 void splash_screen(ssd1306_t *ssd) {
     ssd1306_fill(ssd, 0);
     const char *texto = "FruitLife";
     int char_width = 6, char_height = 8;
     int texto_largura = strlen(texto) * char_width;
     int center_x = SSD1306_WIDTH / 2, center_y = SSD1306_HEIGHT / 2;
     int pos_x = center_x - 32;
     int baseline = (center_y + char_height) / 2;
     int margin = 3;
     int rect_width = texto_largura + margin * 12;
     int rect_height = char_height + margin * 2;
     int rect_x = center_x + 16, rect_y = center_y - 10;
     for (int i = 0; i < 8; i++) {
          ssd1306_fill(ssd, 0);
          ssd1306_draw_string(ssd, texto, pos_x, baseline);
          if (i % 2 == 0) {
              ssd1306_rect(ssd, rect_x, rect_y, rect_width, rect_height, true, false);
          }
          ssd1306_send_data(ssd);
          sleep_ms(500);
     }
     play_startup_music();
 }
   
 //===============================================
 // Callback para os botões (debounce e modo setpoint)
 //===============================================
 void button_callback(uint gpio, uint32_t events) {
     uint32_t current_time = to_ms_since_boot(get_absolute_time());
     if (current_time - last_button_interrupt_time < DEBOUNCE_DELAY_MS) return;
     last_button_interrupt_time = current_time;
     
     if ((gpio == BUTTON_SET) && (events & GPIO_IRQ_EDGE_FALL)) {
         if (!in_set_mode) {
             in_set_mode = true;
             current_set_param = 0;
         } else {
             if (menu_index == 0 || menu_index == 1) {
                 if (current_set_param == 0)
                     current_set_param = 1;
                 else
                     in_set_mode = false;
             } else {
                 in_set_mode = false;
             }
         }
         return;
     }
     
     if (in_set_mode) {
         if ((gpio == BUTTON_NEXT) && (events & GPIO_IRQ_EDGE_FALL)) {
             if (menu_index == 0) {
                 if (current_set_param == 0)
                     etileno_lower += 0.1f;
                 else
                     etileno_upper += 0.1f;
             } else if (menu_index == 1) {
                 if (current_set_param == 0)
                     temp_lower += 0.5f;
                 else
                     temp_upper += 0.5f;
             } else if (menu_index == 2) {
                 umidade_set += 1.0f;
             } else if (menu_index == 3) {
                 co2_set += 50.0f;
             }
             return;
         }
         if ((gpio == BUTTON_BACK) && (events & GPIO_IRQ_EDGE_FALL)) {
             if (menu_index == 0) {
                 if (current_set_param == 0)
                     etileno_lower -= 0.1f;
                 else
                     etileno_upper -= 0.1f;
             } else if (menu_index == 1) {
                 if (current_set_param == 0)
                     temp_lower -= 0.5f;
                 else
                     temp_upper -= 0.5f;
             } else if (menu_index == 2) {
                 umidade_set -= 1.0f;
             } else if (menu_index == 3) {
                 co2_set -= 50.0f;
             }
             return;
         }
         
         if ((gpio == BUTTON_NEXT) && (events & GPIO_IRQ_EDGE_FALL)) {
             menu_index = (menu_index + 1) % 5;
             return;
         }
         if ((gpio == BUTTON_BACK) && (events & GPIO_IRQ_EDGE_FALL)) {
             menu_index = (menu_index + 4) % 5;
             return;
         }
     } else {
         if ((gpio == BUTTON_NEXT) && (events & GPIO_IRQ_EDGE_FALL)) {
             menu_index = (menu_index + 1) % 5;
             return;
         }
         if ((gpio == BUTTON_BACK) && (events & GPIO_IRQ_EDGE_FALL)) {
             menu_index = (menu_index + 4) % 5;
             return;
         }
     }
 }
   
 //===============================================
 // Callback para parar o tom (não bloqueante)
 //===============================================
 int64_t stop_tone_callback(alarm_id_t id, void *user_data) {
     uint gpio = *(uint*)user_data;
     uint slice_num = pwm_gpio_to_slice_num(gpio);
     pwm_set_enabled(slice_num, false);
     gpio_set_function(gpio, GPIO_FUNC_SIO);
     gpio_set_dir(gpio, GPIO_OUT);
     gpio_put(gpio, 0);
     free(user_data);
     return 0;
 }
   
 //===============================================
 // Função auxiliar: inicia um tom de forma não bloqueante
 //===============================================
 void play_tone_non_blocking(uint gpio, int frequency, int duration_ms) {
     uint slice_num = pwm_gpio_to_slice_num(gpio);
     uint channel = pwm_gpio_to_channel(gpio);
     gpio_set_function(gpio, GPIO_FUNC_PWM);
     float divider = 1.0f;
     uint32_t wrap = (uint32_t)((125000000.0f / (divider * frequency)) - 1);
     pwm_set_clkdiv(slice_num, divider);
     pwm_set_wrap(slice_num, wrap);
     pwm_set_chan_level(slice_num, channel, (wrap + 1) / 2);
     pwm_set_enabled(slice_num, true);
     uint *gpio_ptr = malloc(sizeof(uint));
     *gpio_ptr = gpio;
     add_alarm_in_ms(duration_ms, stop_tone_callback, gpio_ptr, false);
 }
   
 //===============================================
 // Função beep não bloqueante (usa BUZZER2)
 //===============================================
 void beep() {
     play_tone_non_blocking(BUZZER2_PIN, 392, 200);
 }
   
 //===============================================
 // Função para atualizar o display OLED (modo normal e de setpoint)
 //===============================================
 void update_display(ssd1306_t *ssd, float value, const char *unit, const char *status, const char *sensor_name) {
     char line1[32], line2[32], line3[32];
     if (in_set_mode) {
         if (menu_index == 0) {
             if (current_set_param == 0)
                 sprintf(line1, "Set Etileno LOW");
             else
                 sprintf(line1, "Set Etileno HIGH");
             sprintf(line2, "Valor: %.2f ppm", (current_set_param == 0 ? etileno_lower : etileno_upper));
         } else if (menu_index == 1) {
             if (current_set_param == 0)
                 sprintf(line1, "Set Temp LOW");
             else
                 sprintf(line1, "Set Temp HIGH");
             sprintf(line2, "Valor: %.2f C", (current_set_param == 0 ? temp_lower : temp_upper));
         } else if (menu_index == 2) {
             sprintf(line1, "Set Umidade");
             sprintf(line2, "Valor: %.2f %%", umidade_set);
         } else if (menu_index == 3) {
             if (current_set_param == 0)
                 sprintf(line1, "Set CO2 BAIXO");
             else
                 sprintf(line1, "Set CO2 ALTO");
             sprintf(line2, "Valor: %.2f ppm", (current_set_param == 0 ? 400.0f : co2_set));
         }
         sprintf(line3, "Pressione SET para salvar");
     } else {
         sprintf(line1, "%s", sensor_name);
         sprintf(line2, "Valor: %.2f %s", value, unit);
         sprintf(line3, "Status: %s", status);
     }
     
     ssd1306_fill(ssd, 0);
     ssd1306_draw_string(ssd, line1, 0, 0);
     ssd1306_draw_string(ssd, line2, 0, 20);
     ssd1306_draw_string(ssd, line3, 0, 40);
     ssd1306_send_data(ssd);
 }
   
 //===============================================
 // Função para atualizar o display OLED no modo Médias
 //===============================================
 void update_display_medias(ssd1306_t *ssd, float media_etileno, float media_temp, float media_umidade, float media_co2, float tempo) {
     char line1[32], line2[32], line3[32];
     sprintf(line1, "Et:%.1fppm T:%.1fC", media_etileno, media_temp);
     sprintf(line2, "Um:%.1f%% CO2:%.0f", media_umidade, media_co2);
     sprintf(line3, "Tempo:%.0fs", tempo);
     ssd1306_fill(ssd, 0);
     ssd1306_draw_string(ssd, line1, 0, 0);
     ssd1306_draw_string(ssd, line2, 0, 20);
     ssd1306_draw_string(ssd, line3, 0, 40);
     ssd1306_send_data(ssd);
 }
   
 //===============================================
 // Função principal
 //===============================================
 int main() {
     stdio_init_all();
     
     // Inicializa OLED
     i2c_init(i2c1, 400 * 1000);
     gpio_set_function(SDA, GPIO_FUNC_I2C);
     gpio_set_function(SCL, GPIO_FUNC_I2C);
     gpio_pull_up(SDA);
     gpio_pull_up(SCL);
     
     // Inicializa ADC para os potenciômetros
     adc_init();
     adc_gpio_init(POT_ETILENO_PIN);
     adc_gpio_init(POT_UMIDADE_PIN);
     // Inicializa sensor CO2 (simulado; você deve definir POT_CO2_PIN no seu hardware)
     adc_gpio_init(POT_ETILENO_PIN);  // Reutiliza POT_ETILENO_PIN para simulação de CO₂
     
     // Inicializa botões
     gpio_init(BUTTON_NEXT);
     gpio_set_dir(BUTTON_NEXT, GPIO_IN);
     gpio_pull_up(BUTTON_NEXT);
     
     gpio_init(BUTTON_BACK);
     gpio_set_dir(BUTTON_BACK, GPIO_IN);
     gpio_pull_up(BUTTON_BACK);
     
     gpio_init(BUTTON_SET);
     gpio_set_dir(BUTTON_SET, GPIO_IN);
     gpio_pull_up(BUTTON_SET);
     
     gpio_set_irq_enabled_with_callback(BUTTON_NEXT, GPIO_IRQ_EDGE_FALL, true, button_callback);
     gpio_set_irq_enabled(BUTTON_BACK, GPIO_IRQ_EDGE_FALL, true);
     gpio_set_irq_enabled(BUTTON_SET, GPIO_IRQ_EDGE_FALL, true);
     
     // Inicializa buzzers
     gpio_init(BUZZER1_PIN);
     gpio_set_dir(BUZZER1_PIN, GPIO_OUT);
     gpio_put(BUZZER1_PIN, 0);
     
     gpio_init(BUZZER2_PIN);
     gpio_set_dir(BUZZER2_PIN, GPIO_OUT);
     gpio_put(BUZZER2_PIN, 0);
     
     init_rgb_led();
     
     // Inicializa OLED
     ssd1306_t ssd;
     ssd1306_init(&ssd, SSD1306_WIDTH, SSD1306_HEIGHT, false, I2C_ADDR, i2c1);
     ssd1306_config(&ssd);
     ssd1306_fill(&ssd, 0);
     ssd1306_send_data(&ssd);
     
     splash_screen(&ssd);
     
     // Inicializa os WS2812 via PIO (pino 7)
     PIO pio = pio0;
     uint sm = 0;
     uint offset = pio_add_program(pio, &ws2812_program);
     ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
     
     // Em modo de configuração, exibe o dígito atual (padrão digital)

     definir_leds(COR_WS2812_R, COR_WS2812_G, COR_WS2812_B);
     
     // Inicializa acumuladores para médias e registra o tempo inicial
     sample_count = 0;
     sum_etileno = 0.0f;
     sum_temp = 0.0f;
     sum_umidade = 0.0f;
     sum_co2 = 0.0f;
     start_time = get_absolute_time();
     
     absolute_time_t proximo_toggle = get_absolute_time();
     bool estado_led = false;
     
     while (true) {
         absolute_time_t agora = get_absolute_time();
         
         // Para os modos 0 (Gás Etileno) e 3 (CO₂), usamos LED piscante
         if (menu_index != 1 && menu_index != 2 && menu_index != 4) {
             if (absolute_time_diff_us(proximo_toggle, agora) >= (INTERVALO_PISCA_LED_MS * 1000)) {
                 estado_led = !estado_led;
                 gpio_put(R_LED_PIN, estado_led);
                 proximo_toggle = delayed_by_ms(proximo_toggle, INTERVALO_PISCA_LED_MS);
             }
         }
         
         if (atualizar_exibicao) {
             // Modo de configuração: exibe o dígito (padrão digital)

             definir_leds(COR_WS2812_R, COR_WS2812_G, COR_WS2812_B);
             atualizar_exibicao = false;
         } else {
             // Modo normal: realiza leituras para todos os sensores
             adc_select_input(POT_ETILENO_PIN - 26);
             int adc_etileno = adc_read();
             float medida_etileno = (adc_etileno / 4095.0f) * 10.0f;
             
             adc_select_input(POT_UMIDADE_PIN - 26);
             int adc_umidade = adc_read();
             float medida_temp = (adc_umidade / 4095.0f) * 40.0f;  // Temperatura simulada
             float medida_umidade = (adc_umidade / 4095.0f) * 100.0f; // Umidade simulada
             
             adc_select_input(POT_ETILENO_PIN - 26);
             int adc_co2 = adc_read();
             float medida_co2 = (adc_co2 / 4095.0f) * 1000.0f;
             
             // Atualiza acumuladores para médias
             sample_count++;
             sum_etileno += medida_etileno;
             sum_temp += medida_temp;
             sum_umidade += medida_umidade;
             sum_co2 += medida_co2;
             
             float valor_medido = 0.0f;
             char status[32];
             char unidade[8];
             
             if (menu_index == 0) { // Gás Etileno
                 valor_medido = medida_etileno;
                 strcpy(unidade, "ppm");
                 if (valor_medido < etileno_lower)
                     strcpy(status, "Normal");
                 else if (valor_medido < etileno_upper)
                     strcpy(status, "Amadurec. rapido");
                 else
                     strcpy(status, "Apodrecendo");
             } else if (menu_index == 1) { // Temperatura
                 valor_medido = medida_temp;
                 strcpy(unidade, "°C");
                 if (valor_medido >= temp_lower && valor_medido <= temp_upper)
                     strcpy(status, "Ideal");
                 else if (valor_medido > temp_upper && valor_medido <= (temp_upper + 5))
                     strcpy(status, "Levemente alto");
                 else if (valor_medido < temp_lower)
                     strcpy(status, "Frio");
                 else
                     strcpy(status, "Critico");
             } else if (menu_index == 2) { // Umidade
                 valor_medido = medida_umidade;
                 strcpy(unidade, "%");
                 if (valor_medido >= umidade_set)
                     strcpy(status, "Ideal");
                 else
                     strcpy(status, "Baixa");
             } else if (menu_index == 3) { // CO₂
                 valor_medido = medida_co2;
                 strcpy(unidade, "ppm");
                 if (valor_medido <= co2_set)
                     strcpy(status, "Ideal");
                 else
                     strcpy(status, "Alto");
             }
             
             // Atualiza o OLED
             if (menu_index == 0)
                 update_display(&ssd, valor_medido, unidade, status, "GAS ETILENO");
             else if (menu_index == 1)
                 update_display(&ssd, valor_medido, unidade, status, "TEMPERATURA");
             else if (menu_index == 2)
                 update_display(&ssd, valor_medido, unidade, status, "UMIDADE");
             else if (menu_index == 3)
                 update_display(&ssd, valor_medido, unidade, status, "CO2");
             else if (menu_index == 4) {
                 float media_etileno = sum_etileno / sample_count;
                 float media_temp = sum_temp / sample_count;
                 float media_umidade = sum_umidade / sample_count;
                 float media_co2 = sum_co2 / sample_count;
                 float tempo = absolute_time_diff_us(start_time, get_absolute_time()) / 1000000.0f;
                 update_display_medias(&ssd, media_etileno, media_temp, media_umidade, media_co2, tempo);
             }
             
             // Atualiza o LED indicador
             if (menu_index == 0) { // Gás Etileno
                 if (valor_medido < etileno_lower)
                     set_rgb_color(0, 255, 0);
                 else if (valor_medido < etileno_upper)
                     set_rgb_color(255, 165, 0);
                 else
                     set_rgb_color(255, 0, 0);
             } else if (menu_index == 3) { // CO₂
                 if (strcmp(status, "Ideal") == 0)
                     set_rgb_color(0, 255, 0);
                 else
                     set_rgb_color(255, 0, 0);
             }
             // Para Temperatura, simula o motor com PWM:
             if (menu_index == 1) {
                 // Se a temperatura estiver na faixa ideal, motor parado: PWM = 0 em ambas as cores (vermelho e azul)
                 if (valor_medido >= temp_lower && valor_medido <= temp_upper) {
                     uint slice_r = pwm_gpio_to_slice_num(R_LED_PIN);
                     uint channel_r = pwm_gpio_to_channel(R_LED_PIN);
                     pwm_set_chan_level(slice_r, channel_r, 0);
                     uint slice_b = pwm_gpio_to_slice_num(B_LED_PIN);
                     uint channel_b = pwm_gpio_to_channel(B_LED_PIN);
                     pwm_set_chan_level(slice_b, channel_b, 0);
                 }
                 else if (valor_medido < temp_lower) {
                     // Temperatura baixa: motor acelerando em vermelho
                     float erro = temp_lower - valor_medido;
                     float max_error = temp_lower; // Exemplo
                     float motor_pwm = (erro / max_error) * PWM_WRAP;
                     if (motor_pwm > PWM_WRAP) motor_pwm = PWM_WRAP;
                     uint slice_r = pwm_gpio_to_slice_num(R_LED_PIN);
                     uint channel_r = pwm_gpio_to_channel(R_LED_PIN);
                     pwm_set_chan_level(slice_r, channel_r, (uint16_t)motor_pwm);
                     // Garante que o LED azul esteja desligado
                     uint slice_b = pwm_gpio_to_slice_num(B_LED_PIN);
                     uint channel_b = pwm_gpio_to_channel(B_LED_PIN);
                     pwm_set_chan_level(slice_b, channel_b, 0);
                 }
                 else { // valor_medido > temp_upper
                     // Temperatura alta: motor acelerando em azul
                     float erro = valor_medido - temp_upper;
                     float max_error = 10.0f;  // Ajustável: consideramos 10°C acima de temp_upper como máximo
                     float motor_pwm = (erro / max_error) * PWM_WRAP;
                     if (motor_pwm > PWM_WRAP) motor_pwm = PWM_WRAP;
                     uint slice_b = pwm_gpio_to_slice_num(B_LED_PIN);
                     uint channel_b = pwm_gpio_to_channel(B_LED_PIN);
                     pwm_set_chan_level(slice_b, channel_b, (uint16_t)motor_pwm);
                     // Garante que o LED vermelho esteja desligado
                     uint slice_r = pwm_gpio_to_slice_num(R_LED_PIN);
                     uint channel_r = pwm_gpio_to_channel(R_LED_PIN);
                     pwm_set_chan_level(slice_r, channel_r, 0);
                 }
             }
             // Para Umidade, o LED indicador (R_LED_PIN) recebe PWM proporcional
             else if (menu_index == 2) {
                 float motor_pwm;
                 if (valor_medido >= umidade_set)
                     motor_pwm = 0;  // Ideal: motor parado
                 else {
                     float erro = umidade_set - valor_medido;
                     float max_error = 50.0f;  // Ajustável
                     motor_pwm = (erro / max_error) * PWM_WRAP;
                     if (motor_pwm > PWM_WRAP) motor_pwm = PWM_WRAP;
                 }
                 uint slice = pwm_gpio_to_slice_num(R_LED_PIN);
                 uint channel = pwm_gpio_to_channel(R_LED_PIN);
                 pwm_set_chan_level(slice, channel, (uint16_t)motor_pwm);
             }
             
             // Atualiza a matriz WS2812 com a carinha apropriada
             if (menu_index == 0) {
                 if (valor_medido < etileno_lower)
                     atualizar_buffer_com_carinha(0); // Neutra ou Feliz (conforme lógica desejada)
                 else if (valor_medido >= etileno_upper)
                     atualizar_buffer_com_carinha(1); // Triste
                 else
                     atualizar_buffer_com_carinha(0);
                 if (valor_medido >= etileno_upper)
                     beep();
             } else if (menu_index == 1) {
                 if (valor_medido >= temp_lower && valor_medido <= temp_upper)
                     atualizar_buffer_com_carinha(0);
                 else
                     atualizar_buffer_com_carinha(1);
                 if (strcmp(status, "Ideal") != 0)
                     beep();
             } else if (menu_index == 2) {
                 if (valor_medido >= umidade_set)
                     atualizar_buffer_com_carinha(0);
                 else
                     atualizar_buffer_com_carinha(1);
                 if (strcmp(status, "Ideal") != 0)
                     beep();
             } else if (menu_index == 3) {
                 if (valor_medido <= co2_set)
                     atualizar_buffer_com_carinha(0);
                 else
                     atualizar_buffer_com_carinha(1);
                 if (strcmp(status, "Ideal") != 0)
                     beep();
             }
             if (menu_index != 4) { // Nos modos normais, atualiza a matriz WS2812
                 definir_leds(COR_WS2812_R, COR_WS2812_G, COR_WS2812_B);
             }
         }
         tight_loop_contents();
     }
     
     return 0;
 }
 