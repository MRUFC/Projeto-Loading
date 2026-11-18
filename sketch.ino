//bibliotecas 
#include <Wire.h>
#include<stdio.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>

OneWire barramento(27);
DallasTemperature sensor(&barramento);

LiquidCrystal_I2C  lcd(0x27,20,4);

bool CMDAquecimento = false;

bool falha = false;
unsigned long inicioFalha = 0;

bool bloqueado = false;

void setup() {

  Serial.begin(115200);
  sensor.begin();

  pinMode(26, OUTPUT);
  digitalWrite(26,LOW);

  Wire.begin(21,22);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("Zona de aquecimento");

  lcd.setCursor(0,1);
  lcd.print("Temperatura:");

  lcd.setCursor(18,1);
  lcd.print("C");

  lcd.setCursor(0,2);
  lcd.print("Estado:");

  pinMode(25, INPUT_PULLUP);
}

void loop() {
  
  //inicio do calulo para desligar o sistema
  unsigned long agora = millis();
  Serial.print("Inicio do ciclo atual em ms: ");
  Serial.println(agora);

  // Refaz a descoberta se nenhum sensor foi identificado, corrige bug
  if (sensor.getDeviceCount() == 0) {
    sensor.begin();
  }

  //solicita a medicao por meio da funcao pertencente ao sensor
  sensor.requestTemperatures();
  //pega a temperatura em graus C no sensor de indice X(0 no caso)
  float temperatura = sensor.getTempCByIndex(0);

  unsigned long instanteLeitura = millis();
  
  if(temperatura == DEVICE_DISCONNECTED_C){

    if(!falha){

      inicioFalha=instanteLeitura;
      falha = true;
    }
    Serial.print("tempo em falha(ms): ");
    Serial.println(instanteLeitura - inicioFalha);

    if(instanteLeitura - inicioFalha >= 10000UL){
      
      bloqueado = true;
    }
  }else{
    falha = false;

  }

  // Prepara um campo de seis caracteres para o LCD
  char textoTemperatura[16];

  if (temperatura == DEVICE_DISCONNECTED_C) {
    
    snprintf(textoTemperatura, sizeof(textoTemperatura), "%6s", "--");
  } else {
    
    snprintf(textoTemperatura, sizeof(textoTemperatura), "%6.2f", temperatura);
  }

  lcd.setCursor(12,1);
  lcd.print(textoTemperatura);

  //Decide o comando de aquecimento; falha de leitura tem prioridade sobre a histerese
  if (bloqueado || temperatura == DEVICE_DISCONNECTED_C) {
    
    CMDAquecimento = false;
  } else if (temperatura <= 48) {
    
    CMDAquecimento = true;
  } else if (temperatura >= 50) {
    
    CMDAquecimento = false;
  }
  lcd.setCursor(0,3);
  if(bloqueado){
    lcd.print("Bloqueado: REARME");
  } else{
    lcd.print("                    ");
  }
  
  //rele funcionando
  bool releTravado = (digitalRead(25)==LOW);
  bool aquecendo = CMDAquecimento || releTravado;

  digitalWrite(26,aquecendo ? HIGH:LOW);
  
  Serial.print("Temperatura: ");
  Serial.println(textoTemperatura);

  // Atualiza o estado em de 12 caracteres
  lcd.setCursor(8, 2);

  if (temperatura == DEVICE_DISCONNECTED_C) {
  
    lcd.print("SEM RESPOSTA");
  } else if (temperatura < 30) {
  
    lcd.print("FRIO        ");
  } else if (temperatura > 60) {
  
    lcd.print("QUENTE      ");
  } else {
  
    lcd.print("NORMAL      ");
  }


  //conferindo o motivo de o sistema desligar o aquecimento
  Serial.print("Comando: ");
  Serial.print(CMDAquecimento);
  Serial.print(" Rele travado: ");
  Serial.print(releTravado);
  Serial.print(" Aquecimento: ");
  Serial.println(aquecendo);
  Serial.print("Bloqueado: ");
  Serial.println(bloqueado);
  delay(800);
}
