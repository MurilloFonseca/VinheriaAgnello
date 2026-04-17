# 🍷 Vinheria Agnello – Sistema de Monitoramento de Luminosidade

> **CP01 – Edge Computing & Computer Systems | FIAP – Engenharia de Software (2026)**  
> Equipe: **Colosys**

---

## 📋 Sumário

- [🍷 Vinheria Agnello – Sistema de Monitoramento de Luminosidade](#-vinheria-agnello--sistema-de-monitoramento-de-luminosidade)
  - [📋 Sumário](#-sumário)
  - [📖 Descrição do Projeto](#-descrição-do-projeto)
  - [⚙️ Funcionamento](#️-funcionamento)
  - [🔧 Componentes de Hardware](#-componentes-de-hardware)
  - [🔌 Esquema de Ligação (Pinagem)](#-esquema-de-ligação-pinagem)
    - [LCD 16×2 (modo 4 bits)](#lcd-162-modo-4-bits)
    - [Periféricos](#periféricos)
  - [📦 Dependências de Software](#-dependências-de-software)
  - [🚀 Como Usar](#-como-usar)
    - [1. Clone o repositório](#1-clone-o-repositório)
    - [2. Abra o código na Arduino IDE](#2-abra-o-código-na-arduino-ide)
    - [3. Selecione a placa e a porta](#3-selecione-a-placa-e-a-porta)
    - [4. Faça o upload](#4-faça-o-upload)
    - [5. Observe o comportamento](#5-observe-o-comportamento)
  - [🛠️ Parâmetros Configuráveis](#️-parâmetros-configuráveis)
  - [🚦 Lógica dos Estados do Sistema](#-lógica-dos-estados-do-sistema)
  - [🎨 Animação de Boas-Vindas](#-animação-de-boas-vindas)
  - [🖥️ Simulação no Tinkercad](#️-simulação-no-tinkercad)
  - [📁 Estrutura do Repositório](#-estrutura-do-repositório)
  - [🎬 Vídeo Explicativo](#-vídeo-explicativo)
  - [📝 Licença](#-licença)

---

## 📖 Descrição do Projeto

A **Vinheria Agnello** é uma loja física tradicional que busca expandir seus negócios para o e-commerce. Para garantir a qualidade dos vinhos durante o armazenamento, foi solicitado o desenvolvimento de um **sistema embarcado de monitoramento ambiental**.

A qualidade do vinho é diretamente influenciada por três fatores:

| Fator | Condição Ideal |
|---|---|
| **Luminosidade** | Penumbra constante; raios UV causam reações químicas indesejadas |
| **Temperatura** | ~13 °C; variações acima de 3 °C geram aromas indesejados |
| **Umidade** | ~70 % (entre 60 % e 80 %); extremos ressecam vedantes ou favorecem fungos |

Esta primeira etapa do projeto (**CP01**) concentra-se no **monitoramento de luminosidade**, utilizando um sensor **LDR** conectado ao Arduino ATmega328P. O sistema classifica o nível de luz em três faixas e aciona alertas visuais (LEDs) e sonoros (buzzer), além de exibir o valor em tempo real em um **display LCD 16×2**.

---

## ⚙️ Funcionamento

O sistema opera em um ciclo contínuo:

1. **Leitura do LDR** – São realizadas 100 (número configurável) leituras analógicas consecutivas.
2. **Cálculo da média** – A média aritmética das leituras reduz ruídos e flutuações momentâneas do sensor.
3. **Mapeamento** – O valor bruto do ADC (0–1023) é convertido para uma escala percentual (0–100 %) usando a função `map()`.
4. **Classificação** – O percentual médio é comparado com os limiares configurados (podem ser alterados):
   - **OK** (< 10 %): ambiente em penumbra, ideal para os vinhos.
   - **Alerta** (10 %–19 %): luminosidade elevada; buzzer toca por 3 segundos.
   - **Perigo** (≥ 20 %): luminosidade crítica; buzzer contínuo.
5. **Sinalização** – O LED e o buzzer correspondentes ao estado são ativados.
6. **Exibição no LCD** – O nível percentual atual é atualizado na tela.

---

## 🔧 Componentes de Hardware

| Componente | Quantidade | Descrição |
|---|---|---|
| Arduino Uno (ATmega328P) | 1 | Microcontrolador principal |
| Display LCD 16×2 | 1 | Exibe o nível de luminosidade e a animação de boas-vindas |
| Sensor LDR | 1 | Fotorresistor para medição de luminosidade |
| Resistor 10 kΩ | 1 | Pull-down para o divisor de tensão do LDR |
| LED Verde | 1 | Indica estado OK |
| LED Amarelo | 1 | Indica estado de alerta |
| LED Vermelho | 1 | Indica estado de perigo |
| Resistor 220 Ω | 3 | Limitadores de corrente para os LEDs |
| Buzzer passivo | 1 | Alarme sonoro |
| Potenciômetro 10 kΩ | 1 | Ajuste de contraste do LCD |
| Protoboard + jumpers | — | Montagem do circuito |

---

## 🔌 Esquema de Ligação (Pinagem)

### LCD 16×2 (modo 4 bits)

| Pino LCD | Pino Arduino | Função |
|---|---|---|
| RS | 12 | Seleção de registrador |
| Enable | 11 | Habilitação |
| D4 | 10 | Dado bit 4 |
| D5 | 5 | Dado bit 5 |
| D6 | 4 | Dado bit 6 |
| D7 | 3 | Dado bit 7 |
| RW | GND | Modo escrita (fixo) |
| V0 | Potenciômetro | Contraste |
| VDD / BLA | 5 V | Alimentação |
| VSS / BLK | GND | Terra |

### Periféricos

| Componente | Pino Arduino |
|---|---|
| Buzzer (passivo) | 8 |
| LED Vermelho | 7 |
| LED Amarelo | 9 |
| LED Verde | 6 |
| LDR (via divisor de tensão) | A0 |

---

## 📦 Dependências de Software

| Dependência | Versão | Origem |
|---|---|---|
| **Arduino IDE** | ≥ 1.8 ou 2.x | [arduino.cc](https://www.arduino.cc/en/software) |
| **LiquidCrystal** | Inclusa na IDE | Biblioteca padrão Arduino |

Nenhuma biblioteca externa precisa ser instalada manualmente.

---

## 🚀 Como Usar

### 1. Clone o repositório

```bash
git clone https://github.com/MurilloFonseca/VinheriaAgnello.git
cd VinheriaAgnello
```

### 2. Abra o código na Arduino IDE

Abra o arquivo `vinho.ino` na Arduino IDE (File → Open).

### 3. Selecione a placa e a porta

- **Tools → Board:** Arduino Uno  
- **Tools → Port:** selecione a porta COM correspondente ao seu Arduino.

### 4. Faça o upload

Clique em **Upload** (→) ou pressione `Ctrl + U`.

### 5. Observe o comportamento

- Ao ligar, o LCD exibe a animação do logo **COLOSYS** com o buzzer tocando.
- Em seguida, o display passa a mostrar `luz: XX` (nível percentual).
- Os LEDs e o buzzer respondem automaticamente conforme a luminosidade detectada.

---

## 🛠️ Parâmetros Configuráveis

Os parâmetros abaixo podem ser ajustados diretamente no início do arquivo `vinho.ino`:

```cpp
int yellowThreshold = 10;  // Limiar (%) para acionar alerta amarelo
int redThreshold    = 20;  // Limiar (%) para acionar alerta vermelho
int meanNumber      = 100; // Número de amostras para calcular a média
int buzzerFreq      = 1000;// Frequência do buzzer em Hz
int playAfter       = 50;  // Iterações de cooldown entre disparos do buzzer
int minLightValue   = 200; // Valor mínimo de luminosidade capturado pelo ldr no ambiente
int maxLightValue   = 900; // Valor máximo de luminosidade capturado pelo ldr no ambiente
```

---

## 🚦 Lógica dos Estados do Sistema

```
Nível de Luminosidade (%)
│
├─ < yellowThreshold (< 10 %)
│    → LED VERDE aceso
│    → Buzzer silencioso
│    → LCD: nível atualizado
│
├─ yellowThreshold ≤ luz < redThreshold (10 %–19 %)
│    → LED AMARELO aceso
│    → Buzzer toca 3s (depois aguarda cooldown)
│    → LCD: nível atualizado
│
└─ ≥ redThreshold (≥ 20 %)
     → LED VERMELHO aceso
     → Buzzer contínuo
     → LCD: nível atualizado
```

---

## 🎨 Animação de Boas-Vindas

Ao iniciar, o sistema exibe no LCD uma animação com o logo da equipe **COLOSYS**. As letras são desenhadas com caracteres customizados (CGRAM) e aparecem em sequência com um efeito de perseguição horizontal, da esquerda para a direita. O buzzer permanece ativo durante toda a animação.

---

## 🖥️ Simulação no Tinkercad

A simulação completa do projeto pode ser acessada no Tinkercad:

🔗 **[Link da simulação – Tinkercad](https://www.tinkercad.com/things/iUxnMhCgPhW-vinheria-agnello)**

---

## 📁 Estrutura do Repositório

```
vinheria-agnello/
├── vinho.ino       # Código-fonte principal
└── README.md       # Este arquivo
```

---

## 🎬 Vídeo Explicativo

🔗 **[Link do vídeo – máx. 3 minutos](#)** *(substituir pelo link público do vídeo)*

O vídeo aborda:
- Como o projeto foi implementado
- Dificuldades encontradas e como foram resolvidas
- Demonstração da simulação em funcionamento

---

## 📝 Licença

Projeto acadêmico desenvolvido para a disciplina de **Edge Computing & Computer Systems** da **FIAP – Engenharia de Software (2026)**.
