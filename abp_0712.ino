/* 
PCD	Arduino
MFRC522 ---> Arduino Mega	

Signal	    Pin	  Pin	
RST/Reset	  RST		5 
SPI SS	    SDA   53 
SPI MOSI	  MOSI  51	
SPI MISO	  MISO	50	
SPI SCK	    SCK   52	

*/

#include <LiquidCrystal.h>
#include <SPI.h> //INCLUSÃO DE BIBLIOTECA
#include <Servo.h> //INCLUSÃO DE BIBLIOTECA   
#include <MFRC522.h> //INCLUSÃO DE BIBLIOTECA
#include <ESP8266WiFi.h>
#include <PubSubClient.h>


//define pinos RFID
#define SS_PIN D8 //PINO SDA
#define RST_PIN D3 //PINO DE RESET

MFRC522 rfid(SS_PIN, RST_PIN); //PASSAGEM DE PARÂMETROS REFERENTE AOS PINOS
Servo s; //OBJETO DO TIPO SERVO
int pos = 12; //POSIÇÃO DO SERVO

const char* ssid = "SATC IOT";
const char* password = "IOT2022@";
const char* mqtt_server = "broker.mqtt-dashboard.com";
int i, vagas[4] = { 0, 0, 0, 0 };
float j, credito[2] = { 10.0, 500.0 };
String clienteGlobal = "";
int cliente1OcupandoVaga = 0;
int cliente2OcupandoVaga = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (topic = "dar_credito") {
    Serial.println("Recebeu credito!");
    String creditoAdicionado = "";

    for (int i = 0; i < length; i++) {
      creditoAdicionado = creditoAdicionado + String((char)payload[i]);
    }

    if (clienteGlobal == "DB:E6:24:1B") {
      credito[0] += creditoAdicionado.toFloat();
      client.publish("credito_atual", String(credito[0]).c_str());
    } else if (clienteGlobal == "1D:A8:48:83") {
      credito[1] += creditoAdicionado.toFloat();
      client.publish("credito_atual", String(credito[1]).c_str());
    }    
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("noticias_do_esp", "");
      client.publish("publicacao_do_esp", "");
      client.publish("credito_atual", "");
      client.publish("dar_credito", "");
      client.publish("cliente_atual", "");
      // ... and resubscribe
      client.subscribe("dar_credito");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void handleCliente(String strID) {
  clienteGlobal = strID;
  client.publish("cliente_atual", String(clienteGlobal).c_str());
  int vagaOcupada = random(0, 4);

  if (strID == "DB:E6:24:1B" && credito[0] <= 0.0) {
    client.publish("noticias_do_esp", ("Cliente " + String(strID) + " sem crédito!").c_str());
    return;
  } else if (strID == "1D:A8:48:83" && credito[1] <= 0.0) {
    client.publish("noticias_do_esp", ("Cliente " + String(strID) + " sem crédito!").c_str());
    return;
  }

  if (vagas[vagaOcupada] == 1) {
    Serial.println("Vaga preenchida");
    client.publish("noticias_do_esp", ("Vaga " + String(vagaOcupada + 1) + " já ocupada!").c_str());
    client.publish("cliente_atual", String(clienteGlobal).c_str());

    return;
  } else if (strID == "DB:E6:24:1B") {
    vagas[vagaOcupada] = 1;

    cliente1OcupandoVaga = vagaOcupada + 1;
    credito[0] -= 5.0;

    client.publish("noticias_do_esp", ("Cliente " + String(strID) + " entrou e ocupou a vaga " + String(cliente1OcupandoVaga)).c_str());
  } else if (strID == "1D:A8:48:83") {
    vagas[vagaOcupada] = 1;

    cliente2OcupandoVaga = vagaOcupada + 1;
    credito[1] -= 5.0;

    client.publish("noticias_do_esp", ("Cliente " + String(strID) + " entrou e ocupou a vaga " + String(cliente2OcupandoVaga)).c_str());
  }

  String vagasString = "";

  for(int i = 0; i < 4; i++) {
    Serial.println(vagas[i]);
    vagasString.concat(vagas[i]);

    if (i != 3) {
      vagasString.concat(", ");
    }
  }

  const char* b = vagasString.c_str();

  client.publish("publicacao_do_esp", b);
  if (strID == "DB:E6:24:1B") {
    client.publish("credito_atual", String(credito[0]).c_str());
  } else if (strID == "1D:A8:48:83") {
    client.publish("credito_atual", String(credito[1]).c_str());
  }

  delay(300);
}

void setup()
{
  Serial.begin(9600); //INICIALIZA A SERIAL
  SPI.begin(); //INICIALIZA O BARRAMENTO SPI
  rfid.PCD_Init(); //INICIALIZA MFRC522

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}
 
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) //VERIFICA SE O CARTÃO PRESENTE NO LEITOR É DIFERENTE DO ÚLTIMO CARTÃO LIDO. CASO NÃO SEJA, FAZ
    return; //RETORNA PARA LER NOVAMENTE
 
  /***INICIO BLOCO DE CÓDIGO RESPONSÁVEL POR GERAR A TAG RFID LIDA***/

  String strID = "";

  for (byte i = 0; i < 4; i++) {
    strID +=
    (rfid.uid.uidByte[i] < 0x10 ? "0" : "") +
    String(rfid.uid.uidByte[i], HEX) +
    (i!=3 ? ":" : "");
  }
  strID.toUpperCase();
  clienteGlobal = String(strID);
  /***FIM DO BLOCO DE CÓDIGO RESPONSÁVEL POR GERAR A TAG RFID LIDA***/


  Serial.print("Identificador (UID) da tag: "); //IMPRIME O TEXTO NA SERIAL
  Serial.println(strID); //IMPRIME NA SERIAL O UID DA TAG RFID

  if (strID == "DB:E6:24:1B" && cliente1OcupandoVaga != 0) {
    vagas[cliente1OcupandoVaga - 1] = 0;
    
    cliente1OcupandoVaga = 0;

    String vagasString = "";

    for(int i = 0; i < 4; i++) {
      Serial.println(vagas[i]);
      vagasString.concat(vagas[i]);

      if (i != 3) {
        vagasString.concat(", ");
      }
    }

    client.publish("publicacao_do_esp", vagasString.c_str());
  
    client.publish("noticias_do_esp", ("Cliente " + String(strID) + " desocupou a vaga").c_str());

    client.publish("cliente_atual", String(clienteGlobal).c_str());
    
    if (clienteGlobal == "DB:E6:24:1B") {
      client.publish("credito_atual", String(credito[0]).c_str());
    } else if (clienteGlobal == "1D:A8:48:83") {
      client.publish("credito_atual", String(credito[1]).c_str());
    }   
    
    rfid.PICC_HaltA(); //PARADA DA LEITURA DO CARTÃO
    rfid.PCD_StopCrypto1(); //PARADA DA CRIPTOGRAFIA NO PCD
    return;
  }

  if (strID == "1D:A8:48:83" && cliente2OcupandoVaga != 0) {
    vagas[cliente2OcupandoVaga - 1] = 0;
    
    cliente2OcupandoVaga = 0;

    String vagasString = "";

    for(int i = 0; i < 4; i++) {
      Serial.println(vagas[i]);
      vagasString.concat(vagas[i]);

      if (i != 3) {
        vagasString.concat(", ");
      }
    }

    client.publish("publicacao_do_esp", vagasString.c_str());
  
    client.publish("noticias_do_esp", ("Cliente " + String(strID) + " desocupou a vaga").c_str());

    client.publish("cliente_atual", String(clienteGlobal).c_str());
    
    if (strID == "DB:E6:24:1B") {
      client.publish("credito_atual", String(credito[0]).c_str());
    } else if (strID == "1D:A8:48:83") {
      client.publish("credito_atual", String(credito[1]).c_str());
    } 
    
    rfid.PICC_HaltA(); //PARADA DA LEITURA DO CARTÃO
    rfid.PCD_StopCrypto1(); //PARADA DA CRIPTOGRAFIA NO PCD
    return;
  }

  if (strID == "DB:E6:24:1B") {
    handleCliente(strID);
  } else if (strID == "1D:A8:48:83") {
    handleCliente(strID);
  }
    
  rfid.PICC_HaltA(); //PARADA DA LEITURA DO CARTÃO
  rfid.PCD_StopCrypto1(); //PARADA DA CRIPTOGRAFIA NO PCD

}
