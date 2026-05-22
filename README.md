# 🍷 Vinheria Agnello – Sistema de Monitoramento Ambiental

> **CP02 – Edge Computing & Computer Systems | FIAP – Engenharia de Software (2026)**  
> Equipe: **Colosys**

---

## 📋 Sumário

- [🍷 Vinheria Agnello – Sistema de Monitoramento Ambiental](#-vinheria-agnello--sistema-de-monitoramento-ambiental)
  - [📋 Sumário](#-sumário)
  - [📖 Descrição do Projeto](#-descrição-do-projeto)
  - [🆕 Novidades do CP02 em relação ao CP01](#-novidades-do-cp02-em-relação-ao-cp01)
  - [⚙️ Funcionamento Geral](#️-funcionamento-geral)
  - [🔧 Componentes de Hardware](#-componentes-de-hardware)
  - [🔌 Esquema de Ligação (Pinagem)](#-esquema-de-ligação-pinagem)
    - [Sensores e Atuadores](#sensores-e-atuadores)
    - [Botões (INPUT\_PULLUP interno – LOW = pressionado)](#botões-input_pullup-interno--low--pressionado)
    - [Módulos I2C (SDA/SCL)](#módulos-i2c-sdascl)
  - [📦 Dependências de Software](#-dependências-de-software)
  - [🚀 Como Usar](#-como-usar)
    - [1. Clone o repositório](#1-clone-o-repositório)
    - [2. Instale as dependências](#2-instale-as-dependências)
    - [3. Abra o código](#3-abra-o-código)
    - [4. Selecione a placa e a porta](#4-selecione-a-placa-e-a-porta)
    - [5. Faça o upload](#5-faça-o-upload)
    - [6. Observe o comportamento](#6-observe-o-comportamento)
  - [🕹️ Manual de Operação](#️-manual-de-operação)
    - [Tela Principal](#tela-principal)
    - [Alternando a Grandeza em Destaque](#alternando-a-grandeza-em-destaque)
    - [Menu de Configuração](#menu-de-configuração)
      - [Estrutura Completa do Menu](#estrutura-completa-do-menu)
      - [Editando um Valor](#editando-um-valor)
  - [⚙️ Parâmetros Padrão](#️-parâmetros-padrão)
  - [🚦 Lógica dos Estados do Sistema](#-lógica-dos-estados-do-sistema)
  - [💾 Data Logger (EEPROM)](#-data-logger-eeprom)
    - [Visualizando os Logs](#visualizando-os-logs)
  - [📁 Estrutura do Repositório](#-estrutura-do-repositório)
  - [🖥️ Simulação no Wokwi](#️-simulação-no-wokwi)
  - [🎬 Vídeo Explicativo](#-vídeo-explicativo)
  - [📝 Licença](#-licença)

---

## 📖 Descrição do Projeto

A **Vinheria Agnello** é uma loja física tradicional em processo de expansão para o e-commerce. Para garantir a qualidade dos vinhos durante o armazenamento, foi desenvolvido um sistema embarcado de **monitoramento ambiental completo**, instalado no ambiente de guarda dos vinhos.

A qualidade do vinho é diretamente influenciada por três fatores ambientais:

| Fator | Condição Ideal |
|---|---|
| **Luminosidade** | Penumbra constante; raios UV causam reações químicas indesejadas |
| **Temperatura** | ~13 °C (±3 °C); variações maiores geram aromas indesejados |
| **Umidade** | ~70 % (entre 60 % e 80 %); extremos ressecam vedantes ou favorecem fungos |

---

## 🆕 Novidades do CP02 em relação ao CP01

| Funcionalidade | CP01 | CP02 |
|---|---|---|
| Monitoramento de luminosidade | ✅ | ✅ |
| Monitoramento de temperatura | ❌ | ✅ (DHT11) |
| Monitoramento de umidade | ❌ | ✅ (DHT11) |
| Display LCD I2C | ❌ (paralelo) | ✅ (menos fios) |
| Alertas para temperatura e umidade | ❌ | ✅ |
| Data Logger na EEPROM | ❌ | ✅ |
| Relógio em tempo real (RTC DS1307) | ❌ | ✅ |
| Menu de configuração por botões | ❌ | ✅ |
| Configurações persistidas na EEPROM | ❌ | ✅ |
| Média móvel de 10 amostras | ❌ | ✅ |
| Suporte a °C e °F | ❌ | ✅ |
| Suporte a UTC configurável | ❌ | ✅ |
| Calibração automática do LDR | ❌ | ✅ |

---

## ⚙️ Funcionamento Geral

O sistema opera em ciclos contínuos de coleta, cálculo, exibição e verificação:

1. **Leitura dos sensores** – LDR (luminosidade) e DHT11 (temperatura + umidade) são lidos a cada ciclo.
2. **Média móvel** – Cada grandeza acumula até 10 amostras em um buffer circular; a média é calculada a cada iteração para suavizar ruídos e flutuações.
3. **Exibição no LCD** – Os valores médios são exibidos no display I2C. A grandeza em destaque pode ser alternada com os botões UP/DOWN.
4. **Verificação de limiares** – As médias são comparadas com os limiares configurados e o LED RGB e o buzzer são acionados conforme o estado mais crítico detectado.
5. **Data Logger** – Sempre que uma grandeza entra em estado de alerta ou perigo, o evento é registrado na EEPROM com timestamp do RTC.
6. **Log via Serial** – Se habilitado, a tabela completa de eventos da EEPROM é impressa no Monitor Serial a cada ciclo.
7. **Espera responsiva** – O sistema aguarda ~2 s entre ciclos fazendo polling dos botões a cada 1 ms, garantindo que o menu responda imediatamente.

---

## 🔧 Componentes de Hardware

| Componente | Qtd | Descrição |
|---|---|---|
| Arduino Uno (ATmega328P) | 1 | Microcontrolador principal |
| Sensor DHT11 | 1 | Temperatura e umidade (substituir DHT22 do Wokwi) |
| Sensor LDR | 1 | Fotorresistor para medição de luminosidade |
| Resistor 10 kΩ | 1 | Pull-down para o divisor de tensão do LDR |
| Display LCD 16×2 + módulo I2C | 1 | Endereço padrão 0x27 |
| Módulo RTC DS1307 | 1 | Relógio em tempo real com bateria |
| LED RGB (cátodo comum) | 1 | Verde / Amarelo / Vermelho de status |
| Resistor 220 Ω | 3 | Limitadores de corrente do LED RGB |
| Buzzer passivo | 1 | Alarme sonoro |
| Push buttons | 6 | Navegação do menu (UP, DOWN, LEFT, RIGHT, CONFIG, SELECT) |
| Resistor 10 kΩ | 6 | Pull-down para os botões (ou usar INPUT_PULLUP interno) |
| Protoboard + jumpers | — | Montagem do circuito |
| Bateria CR2032 | 1 | Alimentação do RTC quando o Arduino está desligado |

---

## 🔌 Esquema de Ligação (Pinagem)

### Sensores e Atuadores

| Componente | Pino Arduino | Observação |
|---|---|---|
| LDR (divisor de tensão) | A0 | Resistor de 10 kΩ para GND |
| DHT11 (dados) | 2 | Resistor pull-up de 4,7 kΩ para 5 V |
| Buzzer passivo | 4 | — |
| LED azul | 5 | Não utilizado na lógica atual |
| LED verde | 6 | Estado OK |
| LED vermelho | 7 | Estado de perigo |

### Botões (INPUT_PULLUP interno – LOW = pressionado)

| Botão | Pino Arduino | Função |
|---|---|---|
| RIGHT | 8 | Aumentar valor / mover cursor direita |
| DOWN | 9 | Próxima grandeza / mover cursor baixo |
| SELECT | 10 | Confirmar seleção / salvar valor |
| UP | 11 | Grandeza anterior / mover cursor cima |
| LEFT | 12 | Diminuir valor / mover cursor esquerda |
| CONFIG | 13 | Abrir/fechar menu principal |

### Módulos I2C (SDA/SCL)

| Módulo | SDA | SCL | Endereço |
|---|---|---|---|
| LCD 16×2 I2C | A4 | A5 | 0x27 |
| RTC DS1307 | A4 | A5 | 0x68 |

> ⚠️ LCD e RTC compartilham o barramento I2C (pinos A4/A5). Ambos funcionam em paralelo pois possuem endereços distintos.

---

## 📦 Dependências de Software

| Biblioteca | Versão | Como instalar |
|---|---|---|
| **Arduino IDE** | ≥ 1.8 ou 2.x | [arduino.cc/software](https://www.arduino.cc/en/software) |
| **LiquidCrystal_I2C** | Qualquer | Library Manager → "LiquidCrystal I2C" (Frank de Brabander) |
| **RTClib** | ≥ 2.0 | Library Manager → "RTClib" (Adafruit) |
| **DHT sensor library** | ≥ 1.4 | Library Manager → "DHT sensor library" (Adafruit) |
| **Adafruit Unified Sensor** | Qualquer | Dependência do DHT – instalar junto |
| **Wire** | inclusa | Comunicação I2C (já inclusa na IDE) |
| **EEPROM** | inclusa | Memória persistente (já inclusa na IDE) |

> 💡 No Wokwi, as bibliotecas são gerenciadas automaticamente via `libraries.txt`.

---

## 🚀 Como Usar

### 1. Clone o repositório

```bash
git clone https://github.com/MurilloFonseca/VinheriaAgnello.git
cd VinheriaAgnello
```

### 2. Instale as dependências

Na Arduino IDE: **Tools → Manage Libraries** e instale cada biblioteca listada acima.

### 3. Abra o código

Abra o arquivo `colosys.ino` na Arduino IDE (**File → Open**).

### 4. Selecione a placa e a porta

- **Tools → Board:** Arduino Uno
- **Tools → Port:** porta COM do seu Arduino

### 5. Faça o upload

Clique em **Upload** (→) ou pressione `Ctrl + U`.

### 6. Observe o comportamento

- Ao ligar, o buzzer toca e o LCD exibe a animação **COLOSYS**.
- Em seguida, o sistema entra no modo de monitoramento normal.
- O LED e o buzzer respondem automaticamente conforme os sensores.

---

## 🕹️ Manual de Operação

### Tela Principal

```
┌────────────────┐
│ Luz        🌡  │   ← Rótulo da grandeza em destaque + ícone secundário
│ 15%        💧  │   ← Valor em destaque + ícone secundário
└────────────────┘
```

Os ícones nas colunas 12–15 mostram as outras duas grandezas de forma compacta ao lado de seus valores.

### Alternando a Grandeza em Destaque

| Botão | Ação |
|---|---|
| **UP** | Grandeza anterior (ciclo: Luz ← Temp ← Umidade) |
| **DOWN** | Próxima grandeza (ciclo: Luz → Temp → Umidade) |

### Menu de Configuração

Pressione **CONFIG** para abrir o menu principal:

```
┌────────────────┐
│◆UTC    Valor   │
│ Unidade  Log   │
└────────────────┘
```

Use os **botões direcionais** para mover o cursor (◆) entre as opções. Pressione **SELECT** para entrar na opção. Pressione **CONFIG** para sair do menu atual e voltar ao anterior (ou à tela principal).

#### Estrutura Completa do Menu

```
CONFIG
└─ Menu Principal
     ├─ UTC       → Ajusta o fuso horário (UTC-12 a UTC+14)
     ├─ Valor
     │    ├─ Luz       → Min / Perigo (margem de alerta) / Max
     │    ├─ Temp      → Min / Perigo / Max  (em °C ou °F)
     │    └─ Humidade  → Min / Perigo / Max
     ├─ Unidade  → Alterna entre °C e °F
     └─ Log      → Liga/desliga a impressão do log no Monitor Serial
```

#### Editando um Valor

Dentro de qualquer editor de valor:

| Botão | Ação |
|---|---|
| **RIGHT** | Aumenta o valor (+1) |
| **LEFT** | Diminui o valor (−1) |
| **SELECT** | Salva na EEPROM e retorna ao menu |

Todos os valores são **salvos automaticamente na EEPROM** ao pressionar SELECT e persistem após desligar o Arduino.

---

## ⚙️ Parâmetros Padrão

| Parâmetro | Valor Padrão | Descrição |
|---|---|---|
| UTC | 0 | Fuso horário para exibição do log |
| Unidade | °C | Unidade de temperatura |
| Log | ON | Impressão via Serial habilitada |
| Luz mín. | 0 % | Limite mínimo de luminosidade |
| Luz máx. | 30 % | Limite máximo de luminosidade |
| Luz aviso | ± 5 % | Margem de alerta para luz |
| Temp. mín. | 10 °C | Limite mínimo de temperatura |
| Temp. máx. | 20 °C | Limite máximo de temperatura |
| Temp. aviso | ± 2 °C | Margem de alerta para temperatura |
| Umid. mín. | 10 % | Limite mínimo de umidade |
| Umid. máx. | 20 % | Limite máximo de umidade |
| Umid. aviso | ± 2 % | Margem de alerta para umidade |
| Amostras médias | 10 | Tamanho da janela de média móvel |

---

## 🚦 Lógica dos Estados do Sistema

Cada grandeza é classificada independentemente. O LED e o buzzer refletem o **estado mais crítico** entre as três:

```
Para cada grandeza (Luz, Temp, Umidade):
│
├─ valor ≤ mín  ou  valor ≥ máx
│    → Estado: PERIGO (R)
│
├─ mín < valor ≤ (mín + aviso)
│   ou  (máx - aviso) ≤ valor < máx
│    → Estado: ALERTA (Y)
│
└─ (mín + aviso) < valor < (máx - aviso)
     → Estado: OK (G)

Resultado combinado (prioridade: Perigo > Alerta > OK):
│
├─ Qualquer 'R' → LED VERMELHO + Buzzer contínuo + Salva na EEPROM
├─ Qualquer 'Y' → LED AMARELO  + Salva na EEPROM (buzzer silencioso)
└─ Todas 'G'    → LED VERDE    + Buzzer silencioso
```

---

## 💾 Data Logger (EEPROM)

Sempre que o sistema detecta um evento de **alerta ou perigo**, um registro é gravado na EEPROM interna do Arduino com os seguintes dados:

| Campo | Tamanho | Descrição |
|---|---|---|
| Timestamp | 4 bytes | Segundos Unix (hora do RTC + offset UTC) |
| Luminosidade | 2 bytes | Valor em % |
| Temperatura | 2 bytes | Valor × 100 (ex.: 1350 → 13,50 °C) |
| Umidade | 2 bytes | Valor × 100 |

Capacidade total: **80 registros** (buffer circular – os mais antigos são sobrescritos).

### Visualizando os Logs

Abra o **Monitor Serial** na Arduino IDE (**Tools → Serial Monitor**, velocidade: **9600 baud**). Com Log = ON, a tabela de eventos é impressa a cada ciclo:

```
Data stored in EEPROM:
Timestamp               Light   Temperature     Humidity
2026-05-22T14:35:00     35 %    23.50 C         85.00 %
2026-05-22T14:36:02     38 %    24.00 C         86.50 %
```

---

## 📁 Estrutura do Repositório

```
VinheriaAgnello/
├── colosys.ino     # Código-fonte principal (comentado)
└── README.md       # Este arquivo – manual de operação
```

---

## 🖥️ Simulação no Wokwi

🔗 **[Acessar simulação – Wokwi](https://wokwi.com/projects/464417389461024769)**

> ⚠️ O Wokwi não possui o sensor DHT11 nativamente. A simulação utiliza o **DHT22**, que apresenta as mesmas características com maior precisão. No código para hardware real, altere `#define DHTTYPE DHT22` para `#define DHTTYPE DHT11`.

---

## 🎬 Vídeo Explicativo

🔗 **[Link do vídeo – Google Drive](https://drive.google.com/file/d/1s35WXl8DOb_guPy0cR45JmhaJQAc_W1G/view?usp=drive_link)**

O vídeo aborda:
- Implementação e arquitetura do projeto
- Demonstração das funcionalidades principais e do menu
- Diferenciais de UX (software)

---

## 📝 Licença

Projeto acadêmico desenvolvido para a disciplina de **Edge Computing & Computer Systems** da **FIAP – Engenharia de Software (2026)**.
