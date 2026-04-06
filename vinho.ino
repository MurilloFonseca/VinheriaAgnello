/*
 * ============================================================
 *  Vinheria Agnello – Sistema de Monitoramento de Luminosidade
 * ============================================================
 *
 * Projeto:     CP01 – Edge Computing & Computer Systems (2026)
 *              FIAP – Engenharia de Software
 * Equipe:      Colosys
 * Integrantes: Beatriz
 *
 * Descrição:
 *   Monitora a luminosidade do ambiente de armazenamento de vinhos
 *   usando um sensor LDR conectado à entrada analógica A0.
 *   O valor lido é mapeado para uma escala de 0–100% e classificado
 *   em três faixas:
 *     • OK      → LED verde aceso
 *     • Alerta  → LED amarelo + buzzer por 3 s (com cooldown)
 *     • Perigo  → LED vermelho + buzzer contínuo
 *
 *   Ao inicializar, o LCD exibe uma animação com o logo da equipe
 *   (COLOSYS) e, em seguida, passa a mostrar o nível de luz em %.
 *
 * Hardware necessário:
 *   - Arduino Uno (ATmega328P)
 *   - LCD 16×2 (interface paralela 4 bits)
 *   - Sensor LDR + resistor de pull-down
 *   - LED verde  (pino 0)
 *   - LED amarelo (pino 1)
 *   - LED vermelho (pino 7)
 *   - Buzzer passivo (pino 8)
 *
 * Dependência de biblioteca:
 *   - LiquidCrystal (inclusa na IDE Arduino)
 * ============================================================
 */

#include <LiquidCrystal.h>

/* ---------------------------------------------------------
 *  Instância do LCD
 *  Parâmetros: RS, Enable, D4, D5, D6, D7
 *  Pino RW está aterrado (modo escrita).
 * --------------------------------------------------------- */
LiquidCrystal lcd(12, 11, 10, 5, 4, 3, 2);

/* ---------------------------------------------------------
 *  Mapeamento de pinos
 * --------------------------------------------------------- */
const int buzzer    = 8;   // Buzzer passivo (saída PWM via tone())
const int redLed    = 7;   // LED vermelho  – nível de PERIGO
const int yellowLed = 1;   // LED amarelo   – nível de ALERTA
const int greenLed  = 0;   // LED verde     – nível OK
const int ldr       = A0;  // Sensor LDR    – entrada analógica

/* ---------------------------------------------------------
 *  Limiares de luminosidade (escala 0–100 %)
 *  Valores abaixo de yellowThreshold → OK
 *  Entre yellowThreshold e redThreshold → Alerta
 *  A partir de redThreshold            → Perigo
 * --------------------------------------------------------- */
int yellowThreshold = 10;  // % mínima para acionar alerta amarelo
int redThreshold    = 20;  // % mínima para acionar alerta vermelho

/* ---------------------------------------------------------
 *  Configurações de amostragem
 * --------------------------------------------------------- */
int meanNumber  = 100;   // Quantidade de leituras para calcular a média

/* ---------------------------------------------------------
 *  Configurações do Buzzer
 *  Cooldown do buzzer no estado de alerta
 *  O buzzer toca 3 s; depois aguarda 'playAfter' iterações
 *  do loop antes de tocar novamente.
 * --------------------------------------------------------- */
int buzzerFreq  = 1000;  // Frequência do buzzer em Hz
int playCooldown = 0;   // Contador regressivo atual (0 = pode tocar)
int playAfter    = 50;  // Iterações de espera entre disparos do buzzer

/* ============================================================
 *  DEFINIÇÃO DOS CARACTERES PERSONALIZADOS DO LCD
 *
 *  Cada letra do logo "COLOSYS" é desenhada em uma grade 5×8
 *  pixels dividida em dois blocos (superior e inferior), cada
 *  um ocupando uma célula de caractere no LCD 16×2.
 *  Um byte[] de 8 posições descreve as 8 linhas do caractere;
 *  os 5 bits menos significativos de cada byte formam a linha.
 * ============================================================ */

/* --- Letra C ------------------------------------------------ */
byte topLeft[8] = {
  B00000, B00001, B00001, B00001,
  B00000, B00000, B00011, B00111,
};
byte bottomLeft[8] = {
  B00111, B00011, B00000, B00000,
  B00001, B00001, B00001, B00000,
};
byte topMiddle[8] = {
  B00001, B00011, B10011, B11000,
  B10011, B00111, B01111, B01111,
};
byte bottomMiddle[8] = {
  B01111, B01111, B00111, B10011,
  B11000, B10011, B00011, B00001,
};
byte topRight[8] = {
  B00000, B00000, B00000, B00000,
  B11000, B11100, B01100, B00000,
};
byte bottomRight[8] = {
  B00000, B01100, B11100, B11000,
  B00000, B00000, B00000, B00000,
};

/* --- Letra O ------------------------------------------------ */
byte ObottomRight[8] = {
  B00110, B01100, B11100, B11000,
  B00000, B00000, B00000, B00000,
};
byte OtopRight[8] = {
  B00000, B00000, B00000, B00000,
  B11000, B11100, B01100, B00110,
};
byte ObottomLeft[8] = {
  B01100, B00110, B00111, B00011,
  B00000, B00000, B00000, B00000,
};
byte OtopLeft[8] = {
  B00000, B00000, B00000, B00000,
  B00011, B00111, B00110, B01100,
};

/* --- Letra Λ ------------------------------------------------ */
byte LbottomRight[8] = {
  B01100, B01110, B00110, B00111,
  B00000, B00000, B00000, B00000,
};
byte LbottomLeft[8] = {
  B00011, B00111, B01110, B01100,
  B00000, B00000, B00000, B00000,
};
byte LtopRight[8] = {
  B00000, B00000, B00000, B00000,
  B00000, B10000, B11000, B11000,
};
byte LtopLeft[8] = {
  B00000, B00000, B01110, B01011,
  B00011, B00001, B00000, B00001,
};

/* --- Letra S ------------------------------------------------ */
byte StopLeft[8] = {
  B00000, B00000, B00000, B00000,
  B00011, B00111, B00110, B00011,
};
byte StopRight[8] = {
  B00000, B00000, B00000, B00000,
  B11000, B11100, B01100, B00000,
};
byte SbottomLeft[8] = {
  B00000, B00110, B00111, B00011,
  B00000, B00000, B00000, B00000,
};
byte SbottomRight[8] = {
  B11000, B01100, B11100, B11000,
  B00000, B00000, B00000, B00000,
};

/* --- Letra Y ------------------------------------------------ */
byte YtopLeft[8] = {
  B00000, B00000, B00000, B00000,
  B00010, B00110, B00110, B00110,
};
byte YtopRight[8] = {
  B00000, B00000, B00000, B00000,
  B01100, B01100, B01100, B01100,
};
byte YbottomLeft[8] = {
  B00110, B00011, B00000, B00110,
  B00111, B00011, B00000, B00000,
};
byte YbottomRight[8] = {
  B01100, B11000, B01100, B01100,
  B11100, B11000, B00000, B00000,
};

/* ============================================================
 *  FUNÇÕES DE RENDERIZAÇÃO DAS LETRAS NO LCD
 *
 *  Cada função recebe a coluna inicial (parâmetro 'line') e
 *  carrega os caracteres personalizados nas posições 0–7 da
 *  memória CGRAM do LCD, depois os escreve nas linhas 0 e 1.
 *  O LCD só suporta 8 caracteres customizados simultâneos (0–7).
 * ============================================================ */

/* Renderiza a letra 'O' a partir da coluna 'line' */
void printO(int line) {
  lcd.createChar(0, OtopLeft);
  lcd.createChar(1, OtopRight);
  lcd.createChar(2, ObottomLeft);
  lcd.createChar(3, ObottomRight);

  lcd.setCursor(line, 0);
  lcd.write(byte(0));
  lcd.write(byte(1));

  lcd.setCursor(line, 1);
  lcd.write(byte(2));
  lcd.write(byte(3));
}

/* Renderiza a letra 'C' (3 colunas de largura) a partir de 'line' */
void printC(int line) {
  lcd.createChar(0, topLeft);
  lcd.createChar(1, topRight);
  lcd.createChar(2, topMiddle);
  lcd.createChar(3, bottomLeft);
  lcd.createChar(4, bottomRight);
  lcd.createChar(5, bottomMiddle);

  lcd.setCursor(line, 0);
  lcd.write(byte(0));
  lcd.write(byte(2));
  lcd.write(byte(1));

  lcd.setCursor(line, 1);
  lcd.write(byte(3));
  lcd.write(byte(5));
  lcd.write(byte(4));
}

/* Renderiza a letra 'Λ' a partir da coluna 'line' */
void printL(int line) {
  lcd.createChar(4, LbottomLeft);
  lcd.createChar(5, LbottomRight);
  lcd.createChar(6, LtopLeft);
  lcd.createChar(7, LtopRight);

  lcd.setCursor(line, 0);
  lcd.write(byte(6));
  lcd.write(byte(7));

  lcd.setCursor(line, 1);
  lcd.write(byte(4));
  lcd.write(byte(5));
}

/* Renderiza a letra 'S' a partir da coluna 'line' */
void printS(int line) {
  lcd.createChar(4, SbottomLeft);
  lcd.createChar(5, SbottomRight);
  lcd.createChar(6, StopLeft);
  lcd.createChar(7, StopRight);

  lcd.setCursor(line, 0);
  lcd.write(byte(6));
  lcd.write(byte(7));

  lcd.setCursor(line, 1);
  lcd.write(byte(4));
  lcd.write(byte(5));
}

/* Renderiza a letra 'Y' a partir da coluna 'line' */
void printY(int line) {
  lcd.createChar(0, YbottomLeft);
  lcd.createChar(1, YbottomRight);
  lcd.createChar(2, YtopLeft);
  lcd.createChar(3, YtopRight);

  lcd.setCursor(line, 0);
  lcd.write(byte(2));
  lcd.write(byte(3));

  lcd.setCursor(line, 1);
  lcd.write(byte(0));
  lcd.write(byte(1));
}

/*
 * hideLine – Apaga uma coluna de caracteres 
 * (determinada pelo parâmetro 'line') nas duas linhas do LCD.
 * Usada para criar o efeito de rolagem na animação do logo.
 */
void hideLine(int line) {
  lcd.setCursor(line, 0);
  lcd.print(" ");

  lcd.setCursor(line, 1);
  lcd.print(" ");
}

/*
 * showColosys – Animação de boas-vindas.
 *
 * Exibe as letras C-O-Λ-O-S-Y-S do logo em sequência, com
 * um efeito de "perseguição" horizontal: cada letra aparece
 * e a anterior é apagada antes de a próxima ser desenhada.
 * O buzzer toca durante toda a animação (chamado em setup()).
 */
void showColosys() {
  const int delayTime = 300; // ms entre cada letra

  // C (coluna 0)
  printC(0);
  delay(delayTime);
  hideLine(0); hideLine(1); hideLine(2);

  // O (coluna 3)
  printO(3);
  delay(delayTime);

  // L (coluna 5) – apaga O ao mesmo tempo
  printL(5);
  delay(100);
  hideLine(3); hideLine(4);
  delay(delayTime);

  // O (coluna 7) – apaga L ao mesmo tempo
  printO(7);
  delay(100);
  hideLine(5); hideLine(6);
  delay(delayTime);

  // S (coluna 9) – apaga O ao mesmo tempo
  printS(9);
  delay(100);
  hideLine(7); hideLine(8);
  delay(delayTime);

  // Y (coluna 11) – apaga S ao mesmo tempo
  printY(11);
  delay(100);
  hideLine(9); hideLine(10);
  delay(delayTime);

  // S final (coluna 13) – apaga Y ao mesmo tempo
  printS(13);
  delay(100);
  hideLine(11); hideLine(12);
  delay(delayTime);

  // Limpa o último S
  hideLine(13); hideLine(14);
}

/*
 * showLight – Exibe o nível de luminosidade no LCD.
 * Escreve o valor inteiro 'level' (0–100) na linha 0,
 * coluna 5 (logo após o rótulo "luz: " configurado no setup).
 */
void showLight(int level) {
  lcd.setCursor(5, 0);
  lcd.print(level);
}

/* ============================================================
 *  SETUP – Executado uma única vez ao ligar/resetar o Arduino
 * ============================================================ */
void setup() {
  // Configura pinos de saída
  pinMode(buzzer,    OUTPUT);
  pinMode(redLed,    OUTPUT);
  pinMode(yellowLed, OUTPUT);
  pinMode(greenLed,  OUTPUT);

  // Configura pino de entrada do LDR
  pinMode(ldr, INPUT);

  // Inicializa o LCD 16×2
  lcd.begin(16, 2);
  lcd.clear();

  // Exibe animação do logo com buzzer ativo durante a apresentação
  tone(buzzer, buzzerFreq);
  showColosys();
  noTone(buzzer);

  delay(100);

  // Prepara o rótulo fixo na tela principal
  lcd.setCursor(0, 0);
  lcd.print("luz: ");

  // Estado inicial: tudo OK, LED verde aceso
  digitalWrite(greenLed, HIGH);
}

/* ============================================================
 *  FUNÇÕES DE CONTROLE DOS LEDs E BUZZER
 * ============================================================ */

/*
 * resetLights – Apaga todos os LEDs e silencia o buzzer.
 * Chamada antes de cada mudança de estado para evitar
 * que dois LEDs fiquem acesos ao mesmo tempo.
 */
void resetLights() {
  digitalWrite(redLed,    LOW);
  digitalWrite(yellowLed, LOW);
  digitalWrite(greenLed,  LOW);
  noTone(buzzer);
}

/*
 * showAlert – Estado ALERTA (luminosidade entre os limiares).
 *
 * Acende o LED amarelo e dispara o buzzer por 3 segundos.
 * Após o disparo, inicia um cooldown de 'playAfter' iterações
 * para evitar que o buzzer fique tocando repetidamente.
 * O contador playCooldown é decrementado a cada iteração do
 * loop enquanto o sistema permanecer em alerta.
 */
void showAlert() {
  int time = 3 * 1000; // Duração do buzzer: 3 000 ms

  resetLights();
  digitalWrite(yellowLed, HIGH);

  if (!playCooldown) {
    // Cooldown zerado → pode tocar
    tone(buzzer, buzzerFreq);
    delay(time);
    noTone(buzzer);

    playCooldown = playAfter; // Reinicia o cooldown
  } else {
    // Ainda em cooldown → apenas decrementa
    playCooldown -= 1;
  }
}

/*
 * showDanger – Estado PERIGO (luminosidade acima do limiar vermelho).
 *
 * Acende o LED vermelho e mantém o buzzer tocando continuamente
 * enquanto a condição persistir.
 */
void showDanger() {
  resetLights();
  digitalWrite(redLed, HIGH);
  tone(buzzer, buzzerFreq); // Buzzer contínuo em nível de perigo
}

/*
 * showOk – Estado OK (luminosidade abaixo do limiar de alerta).
 *
 * Acende o LED verde, silencia o buzzer e zera o cooldown
 * para que o próximo alerta toque imediatamente se necessário.
 */
void showOk() {
  resetLights();
  digitalWrite(greenLed, HIGH);
  playCooldown = 0; // Reseta cooldown quando retorna ao estado OK
}

/* ============================================================
 *  LOOP PRINCIPAL – Repetido continuamente pelo Arduino
 * ============================================================ */
void loop() {
  int lightMeanLevel = 0;

  /*
   * Coleta 'meanNumber' leituras do LDR e acumula a soma.
   * Cada leitura do ADC (10 bits → 0–1023) é mapeada para
   * a faixa 0–100 % usando map().
   * A média reduz ruído e flutuações momentâneas do sensor.
   */
  for (int i = 0; i < meanNumber; i++) {
    int light      = analogRead(ldr);                    // Leitura bruta do ADC (0–1023)
    int lightLevel = map(light, 0, 1023, 0, 100);        // Converte para percentual (0–100 %)
    lightMeanLevel += lightLevel;
  }

  lightMeanLevel /= meanNumber; // Calcula a média aritmética

  // Atualiza o valor exibido no LCD
  showLight(lightMeanLevel);

  /*
   * Avalia o nível médio de luminosidade e aciona o estado
   * correspondente. A ordem de verificação é importante:
   * vermelho tem prioridade sobre amarelo.
   */
  if      (lightMeanLevel >= redThreshold)    showDanger(); // Perigo: luz alta demais
  else if (lightMeanLevel >= yellowThreshold) showAlert();  // Alerta: luz em nível intermediário
  else                                        showOk();     // OK: luz baixa, ambiente adequado
}
