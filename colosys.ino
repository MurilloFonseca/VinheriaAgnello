/*
 * ============================================================
 *  Vinheria Agnello – Sistema de Monitoramento Ambiental
 * ============================================================
 *
 * Projeto:  CP02 – Edge Computing & Computer Systems (2026)
 *           FIAP – Engenharia de Software
 * Equipe:   Colosys
 * Integrantes: Beatriz dos Santos Silva  - rm573698
 *              Lorenzo Mendes Pena       - rm570036
 *              Maria Clara Ramos Santini - rm573246
 *              Murillo Perez da Fonseca  - rm573674
 *              Raíssa Demczuk Capasso    - rm572166
 *
 * Simulação: https://wokwi.com/projects/464417389461024769
 *
 * Descrição:
 *   Sistema embarcado que monitora luminosidade, temperatura e
 *   umidade do ambiente de armazenamento de vinhos. Cada grandeza
 *   é lida continuamente e classificada em três faixas:
 *     • OK      → LED verde
 *     • Alerta  → LED amarelo (zona de atenção)
 *     • Perigo  → LED vermelho + buzzer contínuo
 *
 *   Funcionalidades adicionais:
 *     - Média móvel de 10 amostras por grandeza
 *     - Data Logger: registros de alertas na EEPROM com timestamp (RTC)
 *     - Log via Serial: exibe todos os registros armazenados
 *     - Menu de configuração navegável por botões (UTC, unidade °C/°F,
 *       limiares de luz/temperatura/umidade, exibição de log)
 *     - Configurações persistidas na EEPROM entre reinicializações
 *     - Display LCD I2C 16×2 com animação do logo COLOSYS na inicialização
 *     - Ícones customizados no LCD para temperatura, umidade e luz
 *
 * Hardware necessário:
 *   - Arduino Uno (ATmega328P)
 *   - Display LCD 16×2 com módulo I2C (endereço 0x27)
 *   - Sensor DHT11 (pino 2)
 *   - Sensor LDR + resistor de pull-down (A0)
 *   - Módulo RTC DS1307 (I2C)
 *   - LED RGB de cátodo comum (pinos 5, 6, 7)
 *   - Buzzer passivo (pino 4)
 *   - 6 push buttons com INPUT_PULLUP (pinos 8–13)
 *
 * Dependências (instalar via Library Manager):
 *   - LiquidCrystal_I2C  (Frank de Brabander)
 *   - RTClib              (Adafruit)
 *   - DHT sensor library  (Adafruit)
 *   - Wire, EEPROM        (inclusas na IDE Arduino)
 * ============================================================
 */

#include <LiquidCrystal_I2C.h>   // Biblioteca para LCD via I2C
#include <RTClib.h>              // Biblioteca para o módulo RTC DS1307
#include <Wire.h>                // Comunicação I2C (LCD + RTC)
#include <EEPROM.h>              // Persistência de dados na memória interna
#include "DHT.h"                 // Biblioteca para o sensor DHT11/DHT22

/* Tipo do sensor de temperatura e umidade.
 * Wokwi usa DHT22; para hardware real substituir por DHT11. */
#define DHTTYPE DHT11

/* Typedef auxiliar para ponteiros de função sem parâmetros,
 * usado para passar callbacks de menu como argumentos. */
typedef void (*ConfigFun)();

/* ============================================================
 *  MAPEAMENTO DE PINOS
 * ============================================================ */
const int ldr = A0;          // Sensor LDR – entrada analógica

const int buzzer_freq = 1000; // Frequência do buzzer em Hz
const int buzzer      = 4;    // Buzzer passivo

// LED RGB (cátodo comum – HIGH = acende)
const int red_led   = 7;
const int green_led = 6;
const int blue_led  = 5;

// Botões de navegação do menu (INPUT_PULLUP: LOW = pressionado)
const int right_btn  = 8;
const int left_btn   = 12;
const int up_btn     = 11;
const int down_btn   = 9;
const int config_btn = 13;  // Abre/fecha o menu principal
const int select_btn = 10;  // Confirma seleção

/* ============================================================
 *  INSTÂNCIAS DOS PERIFÉRICOS
 * ============================================================ */
DHT dht(2, DHTTYPE);                // Sensor de temperatura e umidade no pino 2
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD 16 colunas × 2 linhas, endereço I2C 0x27
RTC_DS1307 RTC;                     // Módulo de relógio em tempo real

/* ============================================================
 *  CONFIGURAÇÕES DO DATA LOGGER (EEPROM)
 *
 *  Cada registro ocupa 10 bytes:
 *    [0–3]  uint32_t  timestamp Unix
 *    [4–5]  int       luminosidade (%)
 *    [6–7]  int       temperatura × 100 (ex.: 1350 = 13,50 °C)
 *    [8–9]  int       umidade × 100
 *
 *  Os 10 primeiros bytes (endereços 0–9) são reservados para
 *  o número mágico de validação da EEPROM.
 *  Registros ocupam endereços 10 até max_record_address.
 *  Quando o buffer circular enche, sobrescreve a partir do início.
 * ============================================================ */
const int record_size       = 10;               // Bytes por registro
const int max_records       = 80;               // Máximo de registros armazenados
const int max_record_address = max_records * record_size + 10; // Endereço limite

/* ============================================================
 *  CONFIGURAÇÕES PERSISTIDAS NA EEPROM
 *
 *  Cada configuração tem um endereço fixo na EEPROM.
 *  O número mágico no endereço 0 indica se a EEPROM já foi
 *  inicializada; caso contrário, valores padrão são gravados.
 * ============================================================ */
const unsigned long magic_number = 4294967290UL; // Sentinela de inicialização da EEPROM

// UTC – offset de fuso horário aplicado ao timestamp exibido no Serial
int utc = 0;
const int utc_addr = 950;

// Unidade de temperatura: 'C' (Celsius) ou 'F' (Fahrenheit)
char unity = 'C';
const int unity_addr = 955;

// Calibração do LDR: valores brutos extremos do ADC esperados no ambiente
const int min_light_value = 8;    // ADC mínimo (ambiente totalmente escuro)
const int max_light_value = 1016; // ADC máximo (ambiente totalmente iluminado)

// Exibição do log via Serial: true = imprime a cada ciclo
bool show_log = true;
const int log_addr = 920;

/* Margens de aviso (zona amarela):
 * A faixa OK vai de threshold_min + warning até threshold_max - warning.
 * Fora dessa faixa interna, mas dentro dos thresholds, → amarelo.
 * Fora dos thresholds → vermelho. */
int light_threshold_warning       = 5;  // Margem de aviso para luz (%)
int temperature_threshold_warning = 2;  // Margem de aviso para temperatura (°C)
int humidity_threshold_warning    = 2;  // Margem de aviso para umidade (%)

const int light_warning_addr       = 965;
const int temperature_warning_addr = 970;
const int humidity_warning_addr    = 975;

// Limiares de luminosidade (%)
int light_threshold_min = 0;
int light_threshold_max = 30;
const int light_min_addr = 980;
const int light_max_addr = 985;

// Limiares de temperatura (°C, internamente sempre em Celsius)
float temperature_threshold_min = 10;
float temperature_threshold_max = 20;
const int temperature_min_addr = 990;
const int temperature_max_addr = 995;

// Limiares de umidade (%)
float humidity_threshold_min = 10;
float humidity_threshold_max = 20;
const int humidity_min_addr = 1000;
const int humidity_max_addr = 1005;

// Ponteiro circular para o próximo endereço de escrita na EEPROM
int current_address = 10;

/* Grandeza atualmente exibida em destaque no LCD:
 *   'L' = Luminosidade | 'T' = Temperatura | 'H' = Umidade */
char current_value = 'L';

/* ============================================================
 *  CARACTERES CUSTOMIZADOS DO LCD (CGRAM – máx. 8 simultâneos)
 *
 *  Cada letra do logo COLOSYS ocupa 2 colunas × 2 linhas no LCD,
 *  totalizando até 4 células de caractere. A grade 5×8 pixels
 *  de cada célula é descrita por 8 bytes; os 5 bits menos
 *  significativos formam cada linha da célula.
 * ============================================================ */

/* --- Letra C --- */
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

/* --- Letra O --- */
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

/* --- Letra L --- */
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

/* --- Letra S --- */
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

/* --- Letra Y --- */
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
 *  ÍCONES DE INTERFACE (UI) DO LCD
 *  Carregados na CGRAM após a animação do logo.
 *    slot 0 → termômetro  (temperatura)
 *    slot 1 → gota d'água (umidade)
 *    slot 2 → sol         (luminosidade)
 *    slot 3 → losango     (indicador de seleção no menu)
 * ============================================================ */
byte temp_symbol[8] = {
  B00100, B01010, B01010, B01010,
  B01010, B10001, B10001, B01110,
};

byte hum_symbol[8] = {
  B00110, B01100, B01110, B11110,
  B11111, B11101, B01110, B00000,
};

byte light_symbol[8] = {
  B00000, B10101, B01110, B11111,
  B11111, B01110, B10101, B00000,
};

byte select_symbol[8] = {
  B00000, B00000, B00100, B01110,
  B01110, B00100, B00000, B00000,
};

/* ============================================================
 *  FUNÇÕES DE RENDERIZAÇÃO DO LOGO NO LCD
 *
 *  Cada função carrega os 4 meios-blocos de uma letra na CGRAM
 *  e os escreve nas duas linhas do display a partir da coluna
 *  indicada por 'line'. A CGRAM suporta apenas 8 chars (0–7)
 *  simultaneamente, por isso cada função reutiliza os slots.
 * ============================================================ */

/* Renderiza a letra 'O' (2 colunas) a partir da coluna 'line' */
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

/* Renderiza a letra 'C' (3 colunas) a partir da coluna 'line' */
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

/* Renderiza a letra 'L' (2 colunas) a partir da coluna 'line' */
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

/* Renderiza a letra 'S' (2 colunas) a partir da coluna 'line' */
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

/* Renderiza a letra 'Y' (2 colunas) a partir da coluna 'line' */
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
 * hideLine – Apaga 2 células consecutivas nas duas linhas do LCD
 * a partir da coluna 'line'. Usada para criar o efeito de
 * perseguição da animação do logo.
 */
void hideLine(int line) {
  lcd.setCursor(line, 0);
  lcd.print(" ");
  lcd.setCursor(line, 1);
  lcd.print(" ");
}

/*
 * show_colosys – Animação do logo COLOSYS.
 *
 * Cada letra aparece no LCD e se desloca horizontalmente para
 * a direita enquanto a anterior é apagada, criando um efeito
 * de scroll. O buzzer deve ser acionado externamente antes
 * desta chamada (em setup()).
 */
void show_colosys() {
  const int delayTime = 300; // ms entre cada letra

  printC(0);
  delay(delayTime);
  hideLine(0); hideLine(1); hideLine(2);

  printO(3);
  delay(delayTime);

  printL(5);
  delay(100);
  hideLine(3); hideLine(4);
  delay(delayTime);

  printO(7);
  delay(100);
  hideLine(5); hideLine(6);
  delay(delayTime);

  printS(9);
  delay(100);
  hideLine(7); hideLine(8);
  delay(delayTime);

  printY(11);
  delay(100);
  hideLine(9); hideLine(10);
  delay(delayTime);

  printS(13);
  delay(100);
  hideLine(11); hideLine(12);
  delay(delayTime);

  hideLine(13); hideLine(14);
}

/* ============================================================
 *  CONVERSÃO DE UNIDADES DE TEMPERATURA
 * ============================================================ */

/* Converte Celsius para Fahrenheit */
float celsius_to_fahrenheit(float c) {
  return (c * 9.0 / 5.0) + 32.0;
}

/* Converte Fahrenheit para Celsius */
float fahrenheit_to_celsius(float f) {
  return (f - 32.0) * 5.0 / 9.0;
}

/* ============================================================
 *  GERENCIAMENTO DO BUFFER CIRCULAR DA EEPROM
 * ============================================================ */

/*
 * get_next_address – Avança o ponteiro de escrita para o próximo
 * bloco de record_size bytes. Ao atingir o limite, reinicia em 10
 * (endereço 0–9 reservado para metadados), implementando o
 * comportamento de buffer circular (FIFO sobrescrito).
 */
void get_next_address() {
  current_address += record_size;

  if (current_address >= max_record_address) {
    current_address = 10; // Volta ao início do buffer circular
  }
}

/* ============================================================
 *  PERSISTÊNCIA DE CONFIGURAÇÕES NA EEPROM
 * ============================================================ */

/*
 * load_config – Carrega ou inicializa as configurações da EEPROM.
 *
 * Verifica o número mágico no endereço 0:
 *   - Encontrado: lê todas as configurações salvas.
 *   - Não encontrado (EEPROM virgem ou corrompida): grava o número
 *     mágico e os valores padrão definidos no topo do arquivo.
 */
void load_config() {
  unsigned long magic = 0;
  EEPROM.get(0, magic);

  if (magic == magic_number) {
    // EEPROM já inicializada → carrega todas as configurações
    Serial.print("Value detected. loading config...");

    EEPROM.get(utc_addr,                 utc);
    EEPROM.get(unity_addr,               unity);
    EEPROM.get(log_addr,                 show_log);
    EEPROM.get(light_warning_addr,       light_threshold_warning);
    EEPROM.get(temperature_warning_addr, temperature_threshold_warning);
    EEPROM.get(humidity_warning_addr,    humidity_threshold_warning);
    EEPROM.get(light_min_addr,           light_threshold_min);
    EEPROM.get(light_max_addr,           light_threshold_max);
    EEPROM.get(temperature_min_addr,     temperature_threshold_min);
    EEPROM.get(temperature_max_addr,     temperature_threshold_max);
    EEPROM.get(humidity_min_addr,        humidity_threshold_min);
    EEPROM.get(humidity_max_addr,        humidity_threshold_max);

    Serial.println("loaded!\n");
  } else {
    // Primeira execução → grava número mágico e valores padrão
    Serial.print("Value not detected. saving default config...");

    EEPROM.put(0,                         magic_number);
    EEPROM.put(utc_addr,                  utc);
    EEPROM.put(unity_addr,                unity);
    EEPROM.put(log_addr,                  show_log);
    EEPROM.put(light_warning_addr,        light_threshold_warning);
    EEPROM.put(temperature_warning_addr,  temperature_threshold_warning);
    EEPROM.put(humidity_warning_addr,     humidity_threshold_warning);
    EEPROM.put(light_min_addr,            light_threshold_min);
    EEPROM.put(light_max_addr,            light_threshold_max);
    EEPROM.put(temperature_min_addr,      temperature_threshold_min);
    EEPROM.put(temperature_max_addr,      temperature_threshold_max);
    EEPROM.put(humidity_min_addr,         humidity_threshold_min);
    EEPROM.put(humidity_max_addr,         humidity_threshold_max);

    Serial.println("saved!\n");
  }
}

/* ============================================================
 *  LEITURA DOS SENSORES
 * ============================================================ */

/*
 * get_light – Lê o LDR e retorna a luminosidade em percentual (0–100%).
 *
 * A leitura bruta do ADC (0–1023) é mapeada de forma invertida:
 * max_light_value (muito iluminado) → 0%   (ambiente escuro para o mapa)
 * min_light_value (muito escuro)    → 100% (máxima leitura de luz)
 * constrain() garante que valores fora dos extremos de calibração
 * não ultrapassem a faixa 0–100.
 */
int get_light() {
  const int light       = analogRead(ldr);
  const int light_level = map(light, max_light_value, min_light_value, 0, 100);
  return constrain(light_level, 0, 100);
}

/* get_temperature – Retorna a temperatura em °C lida pelo DHT11 */
float get_temperature() {
  return dht.readTemperature();
}

/* get_humidity – Retorna a umidade relativa (%) lida pelo DHT11 */
float get_humidity() {
  return dht.readHumidity();
}

/* ============================================================
 *  CONTROLE DO LED RGB
 *  O LED possui cátodo comum: apenas o canal ativo recebe HIGH.
 * ============================================================ */

/* Acende vermelho (estado de PERIGO) */
void red_light() {
  digitalWrite(red_led,   HIGH);
  digitalWrite(green_led, LOW);
  digitalWrite(blue_led,  LOW);
}

/* Acende verde (estado OK) */
void green_light() {
  digitalWrite(red_led,   LOW);
  digitalWrite(green_led, HIGH);
  digitalWrite(blue_led,  LOW);
}

/* Acende amarelo = vermelho + verde (estado de ALERTA) */
void yellow_light() {
  digitalWrite(red_led,   HIGH);
  digitalWrite(green_led, HIGH);
  digitalWrite(blue_led,  LOW);
}

/* ============================================================
 *  CLASSIFICAÇÃO DO STATUS DE CADA GRANDEZA
 *
 *  Retorna:
 *    'G' – OK (dentro da faixa interna segura)
 *    'Y' – Alerta (dentro dos thresholds, mas na margem de aviso)
 *    'R' – Perigo (fora dos thresholds min/max)
 *
 *  Faixa OK:     (min + warning) ≤ valor ≤ (max - warning)
 *  Faixa Alerta: min < valor ≤ (min + warning)
 *                ou (max - warning) ≤ valor < max
 *  Faixa Perigo: valor ≤ min ou valor ≥ max
 * ============================================================ */

char check_light_status(int light) {
  const int warning_min = light_threshold_min + light_threshold_warning;
  const int warning_max = light_threshold_max - light_threshold_warning;

  if (light <= light_threshold_min || light >= light_threshold_max) return 'R';
  else if (light <= warning_min    || light >= warning_max)         return 'Y';
  else                                                               return 'G';
}

char check_temperature_status(float temperature) {
  const float warning_min = temperature_threshold_min + temperature_threshold_warning;
  const float warning_max = temperature_threshold_max - temperature_threshold_warning;

  if (temperature <= temperature_threshold_min || temperature >= temperature_threshold_max) return 'R';
  else if (temperature <= warning_min          || temperature >= warning_max)               return 'Y';
  else                                                                                       return 'G';
}

char check_humidity_status(float humidity) {
  const float warning_min = humidity_threshold_min + humidity_threshold_warning;
  const float warning_max = humidity_threshold_max - humidity_threshold_warning;

  if (humidity <= humidity_threshold_min || humidity >= humidity_threshold_max) return 'R';
  else if (humidity <= warning_min       || humidity >= warning_max)            return 'Y';
  else                                                                           return 'G';
}

/*
 * check_danger – Avalia o status das três grandezas e aciona
 * o LED e o buzzer conforme a prioridade: Perigo > Alerta > OK.
 *
 * Qualquer grandeza em PERIGO → LED vermelho + buzzer contínuo + log na EEPROM.
 * Qualquer grandeza em ALERTA → LED amarelo + log na EEPROM (sem buzzer).
 * Todas em OK                 → LED verde, buzzer silencioso.
 *
 * O registro na EEPROM é feito com timestamp do RTC.
 */
void check_danger(int light, float temperature, float humidity) {
  const char light_status       = check_light_status(light);
  const char temperature_status = check_temperature_status(temperature);
  const char humidity_status    = check_humidity_status(humidity);
  const DateTime now            = RTC.now();

  if (light_status == 'R' || temperature_status == 'R' || humidity_status == 'R') {
    red_light();
    save_to_eeprom(light, temperature, humidity, now); // Grava o evento crítico
    tone(buzzer, buzzer_freq);                         // Buzzer contínuo
  }
  else if (light_status == 'Y' || temperature_status == 'Y' || humidity_status == 'Y') {
    yellow_light();
    save_to_eeprom(light, temperature, humidity, now); // Grava o evento de alerta
    noTone(buzzer);
  }
  else {
    green_light();
    noTone(buzzer);
  }
}

/* ============================================================
 *  INTERFACE DO LCD – TELA PRINCIPAL
 *
 *  O display exibe em destaque a grandeza selecionada (current_value)
 *  e, nos cantos direitos, ícones das outras duas grandezas como
 *  contexto secundário. Os ícones ocupam os slots 0–2 da CGRAM.
 * ============================================================ */

/*
 * show_display – Escreve o rótulo e os ícones da tela principal.
 *
 * Layout (exemplo para current_value = 'L'):
 *   Linha 0: "Luz            [ícone umid]"
 *   Linha 1: "XX%            [ícone temp]"
 *
 * Os valores numéricos são atualizados por update_display().
 */
void show_display() {
  lcd.setCursor(0, 0);

  switch (current_value) {
    case 'L':
      lcd.print("Luz");
      lcd.setCursor(12, 0); lcd.write(byte(1)); // Ícone de umidade (contexto)
      lcd.setCursor(12, 1); lcd.write(byte(0)); // Ícone de temperatura (contexto)
      break;

    case 'T':
      lcd.print("Temperatura");
      lcd.setCursor(12, 0); lcd.write(byte(2)); // Ícone de luz (contexto)
      lcd.setCursor(12, 1); lcd.write(byte(1)); // Ícone de umidade (contexto)
      break;

    case 'H':
      lcd.print("Umidade");
      lcd.setCursor(12, 0); lcd.write(byte(0)); // Ícone de temperatura (contexto)
      lcd.setCursor(12, 1); lcd.write(byte(2)); // Ícone de luz (contexto)
      break;
  }
}

/* ============================================================
 *  SETUP – Executado uma única vez ao ligar/resetar o Arduino
 * ============================================================ */
void setup() {
  // Pinos dos sensores
  pinMode(ldr, INPUT);

  // Pinos de saída
  pinMode(buzzer,    OUTPUT);
  pinMode(red_led,   OUTPUT);
  pinMode(green_led, OUTPUT);
  pinMode(blue_led,  OUTPUT);

  // Botões com resistor interno pull-up (LOW = pressionado)
  pinMode(right_btn,  INPUT_PULLUP);
  pinMode(left_btn,   INPUT_PULLUP);
  pinMode(up_btn,     INPUT_PULLUP);
  pinMode(down_btn,   INPUT_PULLUP);
  pinMode(config_btn, INPUT_PULLUP);
  pinMode(select_btn, INPUT_PULLUP);

  dht.begin();      // Inicializa o sensor DHT11
  lcd.init();       // Inicializa o LCD I2C
  lcd.backlight();  // Liga a luz de fundo do LCD
  RTC.begin();      // Inicializa o RTC

  /* Sincroniza o RTC com a hora de compilação do sketch.
   * Em produção, considerar sincronização externa para maior precisão. */
  RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));

  Serial.begin(9600); // Monitor Serial para debug e exibição dos logs

  load_config(); // Carrega configurações da EEPROM (ou grava padrões)

  // Exibe animação do logo com buzzer ativo durante a apresentação
  tone(buzzer, buzzer_freq);
  show_colosys();
  noTone(buzzer);

  // Carrega os ícones de UI na CGRAM (substitui os chars do logo)
  lcd.createChar(0, temp_symbol);
  lcd.createChar(1, hum_symbol);
  lcd.createChar(2, light_symbol);
  lcd.createChar(3, select_symbol);

  reset_display(); // Limpa o display antes de mostrar a tela principal
  show_display();  // Exibe rótulo e ícones da grandeza selecionada
}

/* ============================================================
 *  UTILITÁRIOS DE DISPLAY
 * ============================================================ */

/*
 * reset_display – Preenche as duas linhas do LCD com espaços,
 * apagando todo conteúdo anterior sem piscar a tela.
 */
void reset_display() {
  lcd.setCursor(0, 0);
  lcd.print("                "); // 16 espaços
  lcd.setCursor(0, 1);
  lcd.print("                ");
}

/*
 * change_main – Alterna a grandeza em destaque no LCD.
 *
 * A ordem de rotação é: Luz → Temperatura → Umidade → Luz (cíclico).
 * direction = 1  → avança (para a próxima grandeza)
 * direction = -1 → recua  (para a grandeza anterior)
 */
void change_main(int direction) {
  delay(100); // Debounce mínimo

  reset_display();

  switch (current_value) {
    case 'L':
      current_value = (direction == 1) ? 'T' : 'H';
      break;
    case 'T':
      current_value = (direction == 1) ? 'H' : 'L';
      break;
    case 'H':
      current_value = (direction == 1) ? 'L' : 'T';
      break;
  }

  show_display();
}

/* ============================================================
 *  SISTEMA DE MENU DE CONFIGURAÇÃO
 *
 *  O menu usa uma grade 2×2 de opções no LCD. O cursor (losango)
 *  indica a opção selecionada. Os botões direcionais movem o
 *  cursor; SELECT confirma; CONFIG fecha o menu.
 *
 *  Posições da grade:
 *    '1' → (col 0, lin 0)   '2' → (col 9, lin 0)
 *    '3' → (col 0, lin 1)   '4' → (col 9, lin 1)
 * ============================================================ */

/*
 * reset_positions – Apaga o ícone de seleção de todas as posições
 * antes de redesenhar na nova posição selecionada.
 */
void reset_positions() {
  lcd.setCursor(0, 0); lcd.print(" ");
  lcd.setCursor(0, 1); lcd.print(" ");
  lcd.setCursor(9, 0); lcd.print(" ");
  lcd.setCursor(9, 1); lcd.print(" ");
}

/*
 * show_selected – Exibe o ícone de seleção (losango) na posição
 * correspondente ao caractere 'current' ('1'–'4').
 */
void show_selected(char current) {
  reset_positions();

  switch (current) {
    case '1': lcd.setCursor(0, 0); lcd.write(byte(3)); break;
    case '2': lcd.setCursor(9, 0); lcd.write(byte(3)); break;
    case '3': lcd.setCursor(0, 1); lcd.write(byte(3)); break;
    case '4': lcd.setCursor(9, 1); lcd.write(byte(3)); break;
  }
}

/*
 * move_selected – Move o cursor de seleção na direção indicada
 * ('U' cima, 'D' baixo, 'L' esquerda, 'R' direita).
 * Retorna o identificador da nova posição selecionada.
 * Movimentos inválidos (ex.: ir para cima da linha 0) são ignorados.
 */
char move_selected(char direction, char current) {
  switch (direction) {
    case 'U':
      switch (current) {
        case '3': show_selected('1'); return '1';
        case '4': show_selected('2'); return '2';
        default:  return current;
      }
    case 'D':
      switch (current) {
        case '1': show_selected('3'); return '3';
        case '2': show_selected('4'); return '4';
        default:  return current;
      }
    case 'L':
      switch (current) {
        case '2': show_selected('1'); return '1';
        case '4': show_selected('3'); return '3';
        default:  return current;
      }
    case 'R':
      switch (current) {
        case '1': show_selected('2'); return '2';
        case '3': show_selected('4'); return '4';
        default:  return current;
      }
  }
  return current;
}

/*
 * show_options – Escreve os rótulos das quatro opções do menu
 * na grade 2×2. Posições fixas no LCD:
 *   fst → col 1, lin 0 | snd → col 10, lin 0
 *   trd → col 1, lin 1 | fth → col 10, lin 1
 */
void show_options(String fst, String snd, String trd, String fth) {
  lcd.setCursor(1, 0);  lcd.print(fst);
  lcd.setCursor(10, 0); lcd.print(snd);
  lcd.setCursor(1, 1);  lcd.print(trd);
  lcd.setCursor(10, 1); lcd.print(fth);
}

/*
 * show_config – Exibe um menu 2×2 genérico e aguarda interação.
 *
 * Parâmetros:
 *   fst..fth        – Rótulos das quatro opções
 *   fst_fn..fth_fn  – Callbacks executados ao selecionar cada opção
 *
 * Comportamento:
 *   - Botões direcionais: movem o cursor entre as opções
 *   - SELECT: executa o callback da opção selecionada e reexibe o menu
 *   - CONFIG: sai do menu e restaura os ícones de UI na CGRAM
 */
void show_config(
  String fst, String snd, String trd, String fth,
  ConfigFun fst_fn, ConfigFun snd_fn, ConfigFun trd_fn, ConfigFun fth_fn
) {
  delay(100);
  reset_display();
  show_options(fst, snd, trd, fth);

  char selected = '1';
  show_selected(selected);

  while (true) {
    const int config_pressed = !digitalRead(config_btn);
    const int up_pressed     = !digitalRead(up_btn);
    const int down_pressed   = !digitalRead(down_btn);
    const int left_pressed   = !digitalRead(left_btn);
    const int right_pressed  = !digitalRead(right_btn);
    const int select_pressed = !digitalRead(select_btn);

    // Navegação direcional com debounce embutido
    if (up_pressed)    { delay(150); selected = move_selected('U', selected); }
    if (down_pressed)  { delay(150); selected = move_selected('D', selected); }
    if (right_pressed) { delay(150); selected = move_selected('R', selected); }
    if (left_pressed)  { delay(150); selected = move_selected('L', selected); }

    if (select_pressed) {
      // Executa o callback da opção selecionada
      switch (selected) {
        case '1': fst_fn(); break;
        case '2': snd_fn(); break;
        case '3': trd_fn(); break;
        case '4': fth_fn(); break;
      }

      // Recarrega o ícone de seleção (pode ter sido sobrescrito pelo callback)
      lcd.createChar(3, select_symbol);

      // Reexibe o menu atual após o retorno do callback
      reset_display();
      show_options(fst, snd, trd, fth);
      show_selected(selected);
    }

    if (config_pressed) {
      delay(100);

      // Restaura os ícones de UI antes de sair do menu
      lcd.createChar(0, temp_symbol);
      lcd.createChar(1, hum_symbol);
      lcd.createChar(2, light_symbol);
      lcd.createChar(3, select_symbol);

      return; // Retorna ao loop principal
    }
  }
}

/* ============================================================
 *  CALLBACKS DE CONFIGURAÇÃO INDIVIDUAL
 *
 *  Cada função exibe um editor simples no LCD, onde os botões
 *  esquerda/direita alteram o valor e SELECT confirma e salva
 *  na EEPROM. O botão CONFIG ou SELECT retorna ao menu pai.
 * ============================================================ */

/* Placeholder para posições de menu sem ação */
void do_nothing() { return; }

/* --- UTC --- */
void change_utc() {
  delay(100);
  int selected_utc = utc;

  reset_display();
  lcd.setCursor(0, 0);
  lcd.print("UTC: ");
  if (selected_utc >= 0) lcd.print('+');
  lcd.print(selected_utc);

  while (true) {
    const int select_pressed = !digitalRead(select_btn);
    const int right_pressed  = !digitalRead(right_btn);
    const int left_pressed   = !digitalRead(left_btn);

    if (right_pressed && selected_utc < 14) {   // Máximo UTC+14
      delay(150);
      selected_utc++;
      lcd.setCursor(5, 0);
      if (selected_utc >= 0) lcd.print('+');
      lcd.print(selected_utc);
      lcd.print("   "); // Apaga dígitos residuais
    }
    if (left_pressed && selected_utc > -12) {   // Mínimo UTC-12
      delay(150);
      selected_utc--;
      lcd.setCursor(5, 0);
      if (selected_utc >= 0) lcd.print('+');
      lcd.print(selected_utc);
      lcd.print("   ");
    }
    if (select_pressed) {
      utc = selected_utc;
      EEPROM.put(utc_addr, utc); // Persiste o novo valor
      return;
    }
  }
}

/* --- Unidade de temperatura (°C / °F) --- */
void change_unity() {
  delay(100);
  char selected_unity = unity;

  reset_display();
  lcd.setCursor(0, 0);
  lcd.print("Unidade: ");
  lcd.write(0xDF);        // Símbolo °
  lcd.write(selected_unity);

  while (true) {
    const int select_pressed = !digitalRead(select_btn);
    const int right_pressed  = !digitalRead(right_btn);
    const int left_pressed   = !digitalRead(left_btn);

    if (right_pressed || left_pressed) {
      delay(150);
      selected_unity = (selected_unity == 'C') ? 'F' : 'C'; // Alterna C↔F
      lcd.setCursor(10, 0);
      lcd.write(selected_unity);
    }
    if (select_pressed) {
      unity = selected_unity;
      EEPROM.put(unity_addr, unity);
      return;
    }
  }
}

/* --- Exibição do log via Serial (ON/OFF) --- */
void change_log() {
  delay(100);
  bool selected_log = show_log;

  reset_display();
  lcd.setCursor(0, 0);
  lcd.print("Log: ");
  lcd.print(selected_log ? "ON" : "OFF");

  while (true) {
    const int select_pressed = !digitalRead(select_btn);
    const int right_pressed  = !digitalRead(right_btn);
    const int left_pressed   = !digitalRead(left_btn);

    if (right_pressed || left_pressed) {
      delay(150);
      selected_log = !selected_log;
      lcd.setCursor(5, 0);
      lcd.print(selected_log ? "ON " : "OFF");
    }
    if (select_pressed) {
      show_log = selected_log;
      EEPROM.put(log_addr, show_log);
      return;
    }
  }
}

/* --- Limiares de LUMINOSIDADE --- */
void change_min_light() {
  delay(100);
  int val = light_threshold_min;
  reset_display();
  lcd.setCursor(0, 0); lcd.print("Minimo: "); lcd.print(val); lcd.print("%");

  while (true) {
    const int sel   = !digitalRead(select_btn);
    const int right = !digitalRead(right_btn);
    const int left  = !digitalRead(left_btn);

    if (right && val < 100) { delay(150); val++; lcd.setCursor(8, 0); lcd.print(val); lcd.print("%  "); }
    if (left  && val > 0)   { delay(150); val--; lcd.setCursor(8, 0); lcd.print(val); lcd.print("%  "); }
    if (sel) { light_threshold_min = val; EEPROM.put(light_min_addr, light_threshold_min); return; }
  }
}

void change_max_light() {
  delay(100);
  int val = light_threshold_max;
  reset_display();
  lcd.setCursor(0, 0); lcd.print("Maximo: "); lcd.print(val); lcd.print("%");

  while (true) {
    const int sel   = !digitalRead(select_btn);
    const int right = !digitalRead(right_btn);
    const int left  = !digitalRead(left_btn);

    if (right && val < 100) { delay(150); val++; lcd.setCursor(8, 0); lcd.print(val); lcd.print("%  "); }
    if (left  && val > 0)   { delay(150); val--; lcd.setCursor(8, 0); lcd.print(val); lcd.print("%  "); }
    if (sel) { light_threshold_max = val; EEPROM.put(light_max_addr, light_threshold_max); return; }
  }
}

void change_warning_light() {
  delay(100);
  int val = light_threshold_warning;
  reset_display();
  lcd.setCursor(0, 0); lcd.print("Aviso: "); lcd.print(val); lcd.print("%");

  while (true) {
    const int sel   = !digitalRead(select_btn);
    const int right = !digitalRead(right_btn);
    const int left  = !digitalRead(left_btn);

    if (right && val < 100) { delay(150); val++; lcd.setCursor(7, 0); lcd.print(val); lcd.print("%  "); }
    if (left  && val > 0)   { delay(150); val--; lcd.setCursor(7, 0); lcd.print(val); lcd.print("%  "); }
    if (sel) { light_threshold_warning = val; EEPROM.put(light_warning_addr, light_threshold_warning); return; }
  }
}

/* --- Limiares de TEMPERATURA ---
 * Os valores são exibidos e editados na unidade configurada pelo
 * usuário (°C ou °F), mas armazenados internamente sempre em °C. */
void change_min_temp() {
  delay(100);
  int val = (unity == 'C') ? (int)temperature_threshold_min
                           : (int)celsius_to_fahrenheit(temperature_threshold_min);
  reset_display();
  lcd.setCursor(0, 0); lcd.print("Minimo: "); lcd.print(val); lcd.write(0xDF); lcd.write(unity);

  while (true) {
    const int sel   = !digitalRead(select_btn);
    const int right = !digitalRead(right_btn);
    const int left  = !digitalRead(left_btn);

    if (right && val < 100)  { delay(150); val++; lcd.setCursor(8, 0); lcd.print(val); lcd.write(0xDF); lcd.write(unity); lcd.print("   "); }
    if (left  && val > -50)  { delay(150); val--; lcd.setCursor(8, 0); lcd.print(val); lcd.write(0xDF); lcd.write(unity); lcd.print("   "); }
    if (sel) {
      temperature_threshold_min = (unity == 'C') ? (float)val : fahrenheit_to_celsius((float)val);
      EEPROM.put(temperature_min_addr, temperature_threshold_min);
      return;
    }
  }
}

void change_max_temp() {
  delay(100);
  int val = (unity == 'C') ? (int)temperature_threshold_max
                           : (int)celsius_to_fahrenheit(temperature_threshold_max);
  reset_display();
  lcd.setCursor(0, 0); lcd.print("Maximo: "); lcd.print(val); lcd.write(0xDF); lcd.write(unity);

  while (true) {
    const int sel   = !digitalRead(select_btn);
    const int right = !digitalRead(right_btn);
    const int left  = !digitalRead(left_btn);

    if (right && val < 100)  { delay(150); val++; lcd.setCursor(8, 0); lcd.print(val); lcd.write(0xDF); lcd.write(unity); lcd.print("   "); }
    if (left  && val > -50)  { delay(150); val--; lcd.setCursor(8, 0); lcd.print(val); lcd.write(0xDF); lcd.write(unity); lcd.print("   "); }
    if (sel) {
      temperature_threshold_max = (unity == 'C') ? (float)val : fahrenheit_to_celsius((float)val);
      EEPROM.put(temperature_max_addr, temperature_threshold_max);
      return;
    }
  }
}

void change_warning_temp() {
  delay(100);
  int val = (unity == 'C') ? temperature_threshold_warning
                           : (int)celsius_to_fahrenheit(temperature_threshold_warning);
  reset_display();
  lcd.setCursor(0, 0); lcd.print("Aviso: "); lcd.print(val); lcd.write(0xDF); lcd.write(unity);

  while (true) {
    const int sel   = !digitalRead(select_btn);
    const int right = !digitalRead(right_btn);
    const int left  = !digitalRead(left_btn);

    if (right && val < 100) { delay(150); val++; lcd.setCursor(7, 0); lcd.print(val); lcd.write(0xDF); lcd.write(unity); lcd.print("   "); }
    if (left  && val > 0)   { delay(150); val--; lcd.setCursor(7, 0); lcd.print(val); lcd.write(0xDF); lcd.write(unity); lcd.print("   "); }
    if (sel) {
      temperature_threshold_warning = (unity == 'C') ? val : (int)fahrenheit_to_celsius((float)val);
      EEPROM.put(temperature_warning_addr, temperature_threshold_warning);
      return;
    }
  }
}

/* --- Limiares de UMIDADE --- */
void change_min_hum() {
  delay(100);
  int val = (int)humidity_threshold_min;
  reset_display();
  lcd.setCursor(0, 0); lcd.print("Minimo: "); lcd.print(val); lcd.print("%");

  while (true) {
    const int sel   = !digitalRead(select_btn);
    const int right = !digitalRead(right_btn);
    const int left  = !digitalRead(left_btn);

    if (right && val < 100) { delay(150); val++; lcd.setCursor(8, 0); lcd.print(val); lcd.print("%  "); }
    if (left  && val > 0)   { delay(150); val--; lcd.setCursor(8, 0); lcd.print(val); lcd.print("%  "); }
    if (sel) { humidity_threshold_min = (float)val; EEPROM.put(humidity_min_addr, humidity_threshold_min); return; }
  }
}

void change_max_hum() {
  delay(100);
  int val = (int)humidity_threshold_max;
  reset_display();
  lcd.setCursor(0, 0); lcd.print("Maximo: "); lcd.print(val); lcd.print("%");

  while (true) {
    const int sel   = !digitalRead(select_btn);
    const int right = !digitalRead(right_btn);
    const int left  = !digitalRead(left_btn);

    if (right && val < 100) { delay(150); val++; lcd.setCursor(8, 0); lcd.print(val); lcd.print("%  "); }
    if (left  && val > 0)   { delay(150); val--; lcd.setCursor(8, 0); lcd.print(val); lcd.print("%  "); }
    if (sel) { humidity_threshold_max = (float)val; EEPROM.put(humidity_max_addr, humidity_threshold_max); return; }
  }
}

void change_warning_hum() {
  delay(100);
  int val = humidity_threshold_warning;
  reset_display();
  lcd.setCursor(0, 0); lcd.print("Aviso: "); lcd.print(val); lcd.print("%");

  while (true) {
    const int sel   = !digitalRead(select_btn);
    const int right = !digitalRead(right_btn);
    const int left  = !digitalRead(left_btn);

    if (right && val < 100) { delay(150); val++; lcd.setCursor(7, 0); lcd.print(val); lcd.print("%  "); }
    if (left  && val > 0)   { delay(150); val--; lcd.setCursor(7, 0); lcd.print(val); lcd.print("%  "); }
    if (sel) { humidity_threshold_warning = val; EEPROM.put(humidity_warning_addr, humidity_threshold_warning); return; }
  }
}

/* ============================================================
 *  SUBMENUS DE CONFIGURAÇÃO (NAVEGAÇÃO HIERÁRQUICA)
 *
 *  Estrutura do menu:
 *    main_config()
 *      ├─ change_utc()
 *      ├─ value_config()
 *      │    ├─ light_config()  → Min / Aviso / Max (luz)
 *      │    ├─ temp_config()   → Min / Aviso / Max (temperatura)
 *      │    └─ hum_config()    → Min / Aviso / Max (umidade)
 *      ├─ change_unity()
 *      └─ change_log()
 * ============================================================ */

void light_config() {
  show_config(
    "Min", "Perigo", "Max", "",
    change_min_light, change_warning_light, change_max_light, do_nothing
  );
}

void temp_config() {
  show_config(
    "Min", "Perigo", "Max", "",
    change_min_temp, change_warning_temp, change_max_temp, do_nothing
  );
}

void hum_config() {
  show_config(
    "Min", "Perigo", "Max", "",
    change_min_hum, change_warning_hum, change_max_hum, do_nothing
  );
}

void value_config() {
  show_config(
    "Luz", "Temp", "Humidade", "",
    light_config, temp_config, hum_config, do_nothing
  );
}

void main_config() {
  show_config(
    "UTC", "Valor", "Unidade", "Log",
    change_utc, value_config, change_unity, change_log
  );
}

/* ============================================================
 *  LEITURA DE ENTRADA DO USUÁRIO (LOOP PRINCIPAL)
 * ============================================================ */

/*
 * get_input – Verifica os botões e executa as ações correspondentes.
 *
 * CONFIG   → Abre o menu principal; ao sair, restaura a tela normal.
 * UP/DOWN  → Alterna a grandeza exibida em destaque no LCD.
 *
 * Esta função é chamada múltiplas vezes dentro do loop() para
 * manter a interface responsiva mesmo durante a coleta de amostras.
 */
void get_input() {
  const int config_pressed = !digitalRead(config_btn);
  const int up_pressed     = !digitalRead(up_btn);
  const int down_pressed   = !digitalRead(down_btn);

  if (config_pressed) {
    main_config();
    reset_display();
    show_display();
  }
  if (up_pressed)   change_main(-1); // Grandeza anterior
  if (down_pressed) change_main(1);  // Próxima grandeza
}

/* ============================================================
 *  ATUALIZAÇÃO DOS VALORES NO LCD
 *
 *  Exibe a grandeza selecionada em destaque (linha 1, esquerda)
 *  e as outras duas como valores compactos nas colunas 14–15
 *  (ao lado dos ícones já posicionados por show_display()).
 * ============================================================ */

/*
 * update_display – Atualiza os valores numéricos das três grandezas.
 *
 * Layout para current_value = 'L' (Luminosidade em destaque):
 *   Linha 0: "Luz            [ícone umidade] [hum%]"
 *   Linha 1: "XX%            [ícone temp]   [temp]"
 *
 * Para 'T' e 'H' o layout é análogo com a grandeza selecionada
 * exibida com mais casas decimais na linha 1 à esquerda.
 */
void update_display(int light, float temperature, float humidity) {
  switch (current_value) {
    case 'L':
      lcd.setCursor(0, 1);
      lcd.print(light); lcd.print("%  ");
      lcd.setCursor(14, 0); lcd.print(humidity, 0);      // Umidade como contexto
      lcd.setCursor(14, 1);
      lcd.print(unity == 'C' ? temperature : celsius_to_fahrenheit(temperature), 0); // Temp como contexto
      break;

    case 'T':
      lcd.setCursor(0, 1);
      lcd.print(unity == 'C' ? temperature : celsius_to_fahrenheit(temperature), 1);
      lcd.write(0xDF); lcd.print(unity == 'C' ? "C" : "F");
      lcd.setCursor(14, 0); lcd.print(light);             // Luz como contexto
      lcd.setCursor(14, 1); lcd.print(humidity, 0);       // Umidade como contexto
      break;

    case 'H':
      lcd.setCursor(0, 1);
      lcd.print(humidity, 1); lcd.print("%  ");
      lcd.setCursor(14, 0);
      lcd.print(unity == 'C' ? temperature : celsius_to_fahrenheit(temperature), 0); // Temp como contexto
      lcd.setCursor(14, 1); lcd.print(light);              // Luz como contexto
      break;
  }
}

/* ============================================================
 *  DATA LOGGER – EEPROM
 * ============================================================ */

/*
 * save_to_eeprom – Grava um registro de evento na EEPROM.
 *
 * Estrutura do registro (10 bytes a partir de current_address):
 *   [+0..+3] uint32_t  timestamp Unix (segundos desde 01/01/1970)
 *   [+4..+5] int       luminosidade em %
 *   [+6..+7] int       temperatura × 100 (ex.: 1350 → 13,50 °C)
 *   [+8..+9] int       umidade × 100
 *
 * Temperatura e umidade são multiplicadas por 100 para preservar
 * 2 casas decimais sem usar float na EEPROM (evita erros de precisão).
 * Após gravar, avança o ponteiro circular com get_next_address().
 */
void save_to_eeprom(int light, float temperature, float humidity, DateTime time) {
  const uint32_t timestamp = time.unixtime();
  const int temp_int = (int)(temperature * 100); // Ex.: 13.50 °C → 1350
  const int hum_int  = (int)(humidity    * 100); // Ex.:  57.0 %  → 5700

  EEPROM.put(current_address,     timestamp); // 4 bytes
  EEPROM.put(current_address + 4, light);     // 2 bytes
  EEPROM.put(current_address + 6, temp_int);  // 2 bytes
  EEPROM.put(current_address + 8, hum_int);   // 2 bytes

  get_next_address(); // Avança o ponteiro circular
}

/*
 * print_log – Exibe via Serial todos os registros válidos da EEPROM.
 *
 * Percorre o buffer circular completo (endereços 10 a max_record_address).
 * Registros com timestamp 0x00000000 ou 0xFFFFFFFF são ignorados
 * (posições vazias ou apagadas).
 * O timestamp é ajustado pelo offset UTC configurado antes da exibição.
 * A temperatura é exibida na unidade configurada (°C ou °F).
 */
void print_log() {
  Serial.println("Data stored in EEPROM:");
  Serial.println("Timestamp\t\tLight\tTemperature\tHumidity");

  for (int address = 10; address < max_record_address; address += record_size) {
    uint32_t timestamp = 0;
    int temp_int = 0, hum_int = 0, light = 0;

    EEPROM.get(address,     timestamp);
    EEPROM.get(address + 4, light);
    EEPROM.get(address + 6, temp_int);
    EEPROM.get(address + 8, hum_int);

    const float temperature = temp_int / 100.0;
    const float humidity    = hum_int  / 100.0;

    // Aplica o offset UTC ao timestamp antes de formatar a data/hora
    const uint32_t adjusted_time = timestamp + (utc * 3600L);

    // Ignora posições vazias (0x00000000) ou não inicializadas (0xFFFFFFFF)
    if (timestamp != 0xFFFFFFFF && timestamp != 0) {
      DateTime dt = DateTime(adjusted_time);
      Serial.print(dt.timestamp(DateTime::TIMESTAMP_FULL));
      Serial.print("\t");
      Serial.print(light);
      Serial.print(" %\t");
      Serial.print(unity == 'C' ? temperature : celsius_to_fahrenheit(temperature));
      Serial.print(unity == 'C' ? " C\t\t" : " F\t\t");
      Serial.print(humidity);
      Serial.println(" %");
    }
  }
}

/* ============================================================
 *  LOOP PRINCIPAL – Média móvel + atualização + verificação
 *
 *  Estratégia de amostragem:
 *    1. A cada iteração, coleta 1 leitura de cada sensor.
 *    2. Armazena em buffers circulares de MEAN_SAMPLES posições.
 *    3. Calcula a média das amostras disponíveis.
 *    4. Atualiza o display e verifica os limiares com a média.
 *    5. Aguarda ~2 s chamando get_input() a cada 1 ms
 *       para manter a interface responsiva durante a espera.
 *
 *  Leituras inválidas do DHT (isnan) são descartadas silenciosamente
 *  para não contaminar a média com valores incorretos.
 * ============================================================ */

const int MEAN_SAMPLES = 10;            // Janela de média móvel
int   light_samples[MEAN_SAMPLES];     // Buffer de amostras de luminosidade
float temp_samples[MEAN_SAMPLES];      // Buffer de amostras de temperatura
float hum_samples[MEAN_SAMPLES];       // Buffer de amostras de umidade
int   sample_index = 0;                // Posição atual no buffer circular
bool  samples_full = false;            // true após o primeiro ciclo completo

void loop() {
  // --- 1. Coleta uma amostra de cada sensor ---
  int   raw_light = get_light();
  float raw_temp  = get_temperature();
  float raw_hum   = get_humidity();

  // Descarta leituras inválidas do DHT (sensor não respondeu)
  if (!isnan(raw_temp) && !isnan(raw_hum)) {
    light_samples[sample_index] = raw_light;
    temp_samples[sample_index]  = raw_temp;
    hum_samples[sample_index]   = raw_hum;
    sample_index++;

    if (sample_index >= MEAN_SAMPLES) {
      sample_index  = 0;
      samples_full  = true; // Buffer completo pela primeira vez
    }
  }

  get_input(); // Verifica botões enquanto o sensor é lido

  // Aguarda pelo menos 1 amostra válida antes de processar
  int count = samples_full ? MEAN_SAMPLES : sample_index;
  if (count == 0) return;

  // --- 2. Calcula as médias ---
  long   light_sum = 0;
  double temp_sum  = 0;
  double hum_sum   = 0;

  for (int i = 0; i < count; i++) {
    light_sum += light_samples[i];
    temp_sum  += temp_samples[i];
    hum_sum   += hum_samples[i];

    get_input(); // Mantém a interface responsiva durante o somatório
  }

  int   avg_light = (int)(light_sum / count);
  float avg_temp  = (float)(temp_sum  / count);
  float avg_hum   = (float)(hum_sum   / count);

  // --- 3. Atualiza display e verifica limiares ---
  update_display(avg_light, avg_temp, avg_hum);
  check_danger(avg_light, avg_temp, avg_hum);

  // --- 4. Imprime log via Serial (se habilitado) ---
  if (show_log) print_log();

  // --- 5. Aguarda ~2 s com polling de botões a cada 1 ms ---
  for (int i = 0; i <= 2000; ++i) {
    get_input();
    delay(1);
  }
}
