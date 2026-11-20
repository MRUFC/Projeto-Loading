// Bibliotecas
#include <Wire.h>
#include <stdio.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>

// Sensor no GPIO27 e LCD I2C com 20 colunas e 4 linhas.
OneWire barramento(27);
DallasTemperature sensor(&barramento);
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Comando do controle de temperatura, antes de considerar o rele travado.
bool CMDAquecimento = false;

// Identificacao e instante inicial da falha continua do sensor.
bool falha = false;
unsigned long inicioFalha = 0;

// Bloqueio de seguranca e acesso ao estado salvo na memoria persistente.
bool bloqueado = true;
Preferences memoria;
bool memoriaDisponivel = false;

// Indica que o botao foi observado solto nas condicoes de rearme.
bool rearmePronto = false;

const int buzzer = 33;

// Inicializacao
void setup() {
  Serial.begin(115200);
  sensor.begin();

  // Mantem a saida do aquecimento desligada durante a inicializacao.
  pinMode(26, OUTPUT);
  digitalWrite(26, LOW);

  // Recupera o bloqueio; sem registro salvo, o valor padrao e true.
  memoriaDisponivel = memoria.begin("protecao", false);

  if (memoriaDisponivel) {
    bloqueado = memoria.getBool("bloqueado", true);
  } else {
    bloqueado = true;
    Serial.println("Falha memoria, sistema bloqueado");
  }

  // Comunicacao I2C e textos fixos do LCD.
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Zona de aquecimento");

  lcd.setCursor(0, 1);
  lcd.print("Temperatura:");

  lcd.setCursor(18, 1);
  lcd.print("C");

  lcd.setCursor(0, 2);
  lcd.print("Estado:");

  // Entradas ativas em LOW: rele travado (25) e rearme (32).
  pinMode(25, INPUT_PULLUP);
  pinMode(32, INPUT_PULLUP);

  pinMode(buzzer, OUTPUT);
}

// Monitoramento e controle
void loop() {
  // Registra o inicio do ciclo para acompanhar sua duracao pelo Serial.
  unsigned long agora = millis();
  Serial.print("Inicio do ciclo atual em ms: ");
  Serial.println(agora);

  // Repete a busca quando nenhum sensor foi identificado.
  if (sensor.getDeviceCount() == 0) {
    sensor.begin();
  }

  // Solicita a conversao e le o primeiro sensor do barramento.
  sensor.requestTemperatures();
  float temperatura = sensor.getTempCByIndex(0);

  unsigned long instanteLeitura = millis();

  // Falha do sensor e bloqueio apos 10 segundos de leituras invalidas.
  if (temperatura == DEVICE_DISCONNECTED_C) {
    if (!falha) {
      inicioFalha = instanteLeitura;
      falha = true;
    }

    Serial.print("tempo em falha(ms): ");
    Serial.println(instanteLeitura - inicioFalha);

    // Grava o bloqueio apenas na transicao de liberado para bloqueado.
    if (instanteLeitura - inicioFalha >= 10000UL && !bloqueado) {
      bloqueado = true;
      digitalWrite(26, LOW);

      if (!memoriaDisponivel || memoria.putBool("bloqueado", true) != 1) {
        memoriaDisponivel = false;
        Serial.println("Falha ao salvar o bloqueio");
      }
    }
  } else {
    // A recuperacao do sensor nao remove um bloqueio ja ocorrido.
    falha = false;
  }

  // Temperatura no LCD: largura minima de seis caracteres ou "--" na falha.
  char textoTemperatura[16];

  if (temperatura == DEVICE_DISCONNECTED_C) {
    snprintf(textoTemperatura, sizeof(textoTemperatura), "%6s", "--");
  } else {
    snprintf(textoTemperatura, sizeof(textoTemperatura), "%6.2f", temperatura);
  }

  lcd.setCursor(12, 1);
  lcd.print(textoTemperatura);

  // Le as entradas antes de decidir se o rearme pode ser aceito.
  bool rearme = (digitalRead(32) == LOW);
  bool releTravado = (digitalRead(25) == LOW);

  // Desarma a permissao se alguma condicao impedir o rearme.
  if (!bloqueado || falha || releTravado || !memoriaDisponivel) {
    rearmePronto = false;

  } else if (!rearme) {
    // Sensor valido, rele destravado, memoria disponivel e botao solto.
    rearmePronto = true;

  } else if (rearmePronto) {
    // Aceita o pressionamento depois de observar o botao solto.
    rearmePronto = false;

    if (memoria.putBool("bloqueado", false) == 1) {
      bloqueado = false;
    } else {
      memoriaDisponivel = false;
      Serial.println("Falha ao salvar o rearme");
    }
  }

  // Bloqueio e falha tem prioridade sobre o controle por histerese.
  // Entre 48 e 50 graus, sem incluir os limites, preserva o comando anterior.
  if (bloqueado || temperatura == DEVICE_DISCONNECTED_C) {
    CMDAquecimento = false;
  } else if (temperatura <= 48) {
    CMDAquecimento = true;
  } else if (temperatura >= 50) {
    CMDAquecimento = false;
  }

  // Indicacao de bloqueio na ultima linha do LCD.
  lcd.setCursor(0, 3);

  if (bloqueado) {
    lcd.print("Bloqueado: REARME");
  } else {
    lcd.print("                    ");
  }

  // Saida simulada: rele travado pode manter o aquecimento sem comando.
  // O bloqueio representa um corte independente, inclusive do rele travado.
  bool aquecendo = !bloqueado && (CMDAquecimento || releTravado);

  digitalWrite(26, aquecendo ? HIGH : LOW);

  Serial.print("Temperatura: ");
  Serial.println(textoTemperatura);

  // Estado da leitura no LCD, com prioridade para falha do sensor.
  lcd.setCursor(8, 2);

  if (temperatura == DEVICE_DISCONNECTED_C) {
    lcd.print("FALHA SENSOR");
  } else if (temperatura < 30) {
    lcd.print("ALERTA FRIO ");
  } else if (temperatura > 60) {
    lcd.print("ALERTA CALOR");
  } else {
    lcd.print("NORMAL      ");
  }

  // Alarmes: falha (2000 Hz), calor (1000 Hz) e frio (500 Hz).
  if (falha) {
    tone(buzzer, 2000);
  } else if (temperatura > 60) {
    tone(buzzer, 1000);
  } else if (temperatura < 30) {
    tone(buzzer, 500);
  } else {
    noTone(buzzer);
  }

  // Diagnostico do comando, da saida, das entradas e das protecoes.
  Serial.print("Comando: ");
  Serial.print(CMDAquecimento);
  Serial.print(" Rele travado: ");
  Serial.print(releTravado);
  Serial.print(" Aquecimento: ");
  Serial.println(aquecendo);
  Serial.print("Bloqueado: ");
  Serial.println(bloqueado);
  Serial.print("Estado do botao: ");
  Serial.println(digitalRead(32));

  Serial.print("Falha do sensor: ");
  Serial.println(falha);

  Serial.print("Memoria disponivel: ");
  Serial.println(memoriaDisponivel);

  Serial.print("Rearme pressionado: ");
  Serial.println(rearme);

  // Pausa adicional; o ciclo tambem inclui a conversao do sensor e as saidas.
  delay(800);
}
