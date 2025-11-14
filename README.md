<p align="center">
  <img src="https://capsule-render.vercel.app/api?type=blur&height=470&color=0:FF0000,50:FFFF00,100:00FF00&text=Intelligent%20Traffic%20Lights&textBg=false&section=header&reversal=true&fontColor=FFFFFF&fontSize=40&fontAlign=50&animation=fadeIn&descAlign=16" width="550"/>
</p>


# 🚦 Semáforo Inteligente com LDR e Modo Noturno | Smart City

<p align="center">
 G2: Átila Neto, Anny Cerazi, Eduardo Casarini, Giorgia Scherer, Leonardo Ramos, Lucas Cofcewicz, Rafael Josué
</p>

---

Este repositório apresenta um projeto de "Semáforo Inteligente" capaz de:

- Detectar a presença de veículos usando um **sensor LDR**  
- Adaptar o funcionamento para **modo noturno** automaticamente  
- Permitir controle remoto e visualização via **interface online**  
- Sincronizar dois semáforos simulando um cruzamento real  

---

## 🔧 Parte 1 — Montagem Física 

### ✔ Componentes utilizados
- 2x Semáforos montados com LEDs (Vermelho, Amarelo, Verde)
- 1x Arduino (Uno ou similar)
- 1x LDR (Sensor de luminosidade)
- 1x Resistor de 10kΩ (divisor de tensão)
- Jumpers e protoboard
- Fonte USB

### ✔ Objetivo
O LDR detecta variação de luz simulando:
- **Carro passando** (sombra → baixa luminosidade)  
- **Modo noturno** (ambiente escuro por longo período)

