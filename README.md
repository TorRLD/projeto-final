# Projeto Final - Capacitação Embarcatech: FruitLife Device

Este é o projeto final do curso de capacitação Embarcatech. O dispositivo, denominado **FruitLife Device**, tem como objetivo monitorar e controlar variáveis ambientais críticas para o transporte de frutas climatéricas, tais como gás etileno, temperatura, umidade e dióxido de carbono (CO₂). Além disso, o sistema permite a configuração dos setpoints dos sensores e simula o controle de um motor de refrigeração por meio de um LED RGB operando com PWM.


## Vídeo Demonstração

[Vídeo no Youtube](https://www.youtube.com/watch?v=pe8C2C0KPvE)

---

## Descrição

O FruitLife Device utiliza:

- **OLED (SSD1306):** Exibe os valores medidos e a classificação dos sensores.
- **Matriz WS2812:** Mostra emoticons (feliz, neutra ou triste) conforme o status dos sensores.
- **Potenciômetros:** Simulam os sensores (gás etileno/CO₂ e temperatura/umidade).
- **Botões:** Permitem a navegação no menu e o ajuste dos setpoints.
- **Buzzers:** Emitem alertas sonoros.
- **LED RGB:** Indica o status dos sensores e, em modos específicos (temperatura e umidade), simula o acionamento de um motor de refrigeração. Quando a temperatura ultrapassa o setpoint superior, o canal azul do LED aumenta o PWM para representar o aumento da velocidade do motor de resfriamento.

---

## Pinos Utilizados

- **OLED (SSD1306):**
  - SDA: Pino 14
  - SCL: Pino 15
- **Sensores (Potenciômetros):**
  - POT_ETILENO_PIN (Simula Gás Etileno e CO₂): Pino 27
  - POT_UMIDADE_PIN (Simula Temperatura e Umidade): Pino 26
- **Botões:**
  - BUTTON_NEXT: Pino 5
  - BUTTON_BACK: Pino 6
  - BUTTON_SET: Pino 22
- **Buzzers:**
  - BUZZER1_PIN (Startup/alerta): Pino 10
  - BUZZER2_PIN (Alertas): Pino 21
- **LED RGB (PWM):**
  - R_LED_PIN (Também utilizado para simular o motor): Pino 13
  - G_LED_PIN: Pino 11
  - B_LED_PIN: Pino 12
- **Matriz WS2812:**
  - WS2812_PIN: Pino 7

---

## Periféricos Utilizados

- **I2C:** Utilizado para comunicação com o display OLED.
- **PWM:** Usado para controlar o LED RGB e simular o motor de refrigeração.
- **ADC:** Responsável pela leitura dos potenciômetros que simulam os sensores.
- **GPIO:** Gerencia os botões e o acionamento dos buzzers.

---

## Funcionamento

### Modo Normal

- O OLED exibe os valores atuais dos sensores e sua classificação (status).
- A matriz WS2812 mostra emoticons que refletem o estado dos sensores.
- Para os sensores de temperatura e umidade, o LED indicativo (R_LED_PIN) opera via PWM para simular a velocidade de um motor de refrigeração:
  - Se a temperatura estiver dentro do intervalo ideal, o motor (LED) permanece parado (PWM = 0).
  - Se a temperatura for menor que o setpoint inferior, o PWM aumenta no canal vermelho.
  - **Se a temperatura ultrapassar o setpoint superior, o PWM aumenta no canal azul**, indicando o acionamento do motor de resfriamento.
- Para os sensores de gás etileno e CO₂, o LED RGB (usado em modo piscante) indica o status (verde, laranja ou vermelho), e um alerta sonoro (beep) é emitido se o valor estiver acima do setpoint.

### Modo de Configuração

- Ao pressionar BUTTON_SET, o sistema entra no modo de configuração e exibe na matriz WS2812 o dígito atual (em formato digital).
- Os botões BUTTON_NEXT e BUTTON_BACK permitem ajustar os setpoints do sensor ativo.
- O OLED mostra os setpoints atuais para o sensor em configuração.

### Modo de Médias (menu 4)

- Exibe os valores médios acumulados de cada sensor (gás etileno, temperatura, umidade e CO₂) desde o início da operação, bem como o tempo decorrido.

---

## Como Clonar o Repositório

Abra um terminal e execute o seguinte comando:

```bash
git clone https://github.com/TorRLD/projeto-final
```


## Como Executar o Programa

Este projeto foi desenvolvido para ser executado utilizando o plugin para Raspberry Pi Pico no Visual Studio Code. Siga os passos abaixo:

1. **Instale o Visual Studio Code.**
2. **Instale a extensão para Raspberry Pi Pico** (por exemplo, *Pico-Go* ou outra recomendada).
3. **Clone o repositório** conforme as instruções acima.
4. **Abra o projeto no Visual Studio Code.**
5. **Configure o ambiente de desenvolvimento** conforme as instruções do plugin (seleção da placa, SDK, etc.).
6. **Compile o projeto.** O firmware gerado terá a extensão `.uf2`.
7. **Carregue o firmware no Raspberry Pi Pico W** ou utilize a simulação no Wokwi.

---

## Projeto Final do Curso de Capacitação Embarcatech

Este projeto integra tecnologias de monitoramento e controle ambiental em uma única solução, visando a preservação de frutas durante o transporte. O sistema demonstra originalidade ao combinar a leitura de sensores simulados, interface de usuário via OLED, feedback visual com emoticons em uma matriz WS2812 e controle simulado de motor de refrigeração via PWM.

---

## Autor

**Heitor Lemos**


## LICENSE

[LICENSE](LICENSE)
