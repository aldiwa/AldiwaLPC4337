#include "mbed.h"
#include "DHT.h"
#include "Rtc_Ds1307.h"
#include <string>

// Leds da placa LPCXpresso4337
DigitalOut ledR(LED1);
DigitalOut ledG(LED2);
DigitalOut ledB(LED3);

// Enumera os sensores que compõem a solução
enum sensores {
    SENSOR_DHT11 = 1,
    SENSOR_FS300A = 2
};

// Enumera os tipos de registros
enum registros {
    UPT = 0,
    FLX = 1,
    TMP = 2,
    UMD = 3
};

// Comunicação serial com o PC
Serial pc(USBTX, USBRX);

// Sensor de temperatura e umidade
DHT sensorDht11(D4, DHT11);

// Modulo relé (solenoide)
DigitalOut releSolenoide(D6);

// Sensor de relógio de tempo real (RTC)
Rtc_Ds1307 rtc(D14, D15);

// Sensor de fluxo de água
InterruptIn sensorFluxo(D8);

// Botão de modo de configuração
InterruptIn botaoModoConfiguracao(D9);

// Botão de corte do fluxo de água
InterruptIn botaoCorteFluxo(D10);

// Estado do modo de configuração 
int estadoModoConfiguracao = 0;

// Estado do corte do fluxo de água
int estadoCorteFluxo = 0;

// Acumulador da vazão da água em l/min
float vazaoAgua = 0;

// Acumulador da média da vazão de água, a cada 1 minuto
float mediaVazaoAgua = 0;

// Contador de ciclos do sensor de fluxo de água
int ciclosFluxo;

// Contador de segundos decorridos da vazão de água
int segundosVazaoAgua = 0;

// Controle da temperatura atual
float temperaturaAtual = 0;

// Controle da umidade atual
float umidadeAtual = 0;

// Buffer da comunicação serial
char buffer[128];
int ptrLeitura = 0;
    
// Controles de Temperatura e Umidade
int erroDHT11 = 0;
float temperatura = 0.0f;
float umidade = 0.0f;
    
// Tempo de Uptime
time_t diferencaTempoUptime;

// Tempo para captura dos Dados de Temperatura e Umidade
time_t diferencaTempUmidade;

// Tempo para informar Fluxo de Agua
time_t checagemFluxoAgua;
int ativouFluxoAgua = 0;

// Define o modo de configuração (entra ou sai do modo)
int defineModoConfiguracao(int modo) {
    // Atualiza o estado do modo de configuração
    estadoModoConfiguracao = modo;

    if (estadoModoConfiguracao == 1)
    {
        // Liga o Led vermelho, indicando que esta no modo de configuração
        ledR = 1;
    } else {
        // Desliga o Led vermelho, indicando que não esta no modo de configuração
        ledR = 0;
    }
    return 0;
}

// Switch do botão do modo de configuração
void acionamentoBotaoModoConfiguracao() {
    defineModoConfiguracao(!estadoModoConfiguracao);
}

// Define se permite ou não o fluxo de água
int defineFluxoAgua(int fluxo) {
    // Atualiza o estado do fluxo de água
    estadoCorteFluxo = fluxo;

    if (estadoCorteFluxo == 1)
    {
        // Liga o Led verde, indicando que o fluxo de água esta livre
        ledG = 1;
        
        // Liga o relé (solenoide)
        // Por se tratar de um protótipo, estamos utilizando uma solenoide NF (normalmente fechada)
        releSolenoide = 1;
    } else {
        // Desliga o Led verde, indicando que o fluxo esta interrompido
        ledG = 0;
        
        // Desliga o relé (solenoide)
        // Por se tratar de um protótipo, estamos utilizando uma solenoide NF (normalmente fechada)
        releSolenoide = 0;
    }    
    return 0;
}

// Switch do botão de corte do fluxo de água
void acionamentoBotaoCorteFluxo() {
    defineFluxoAgua(!estadoCorteFluxo);
}

// Função de contagem da rotação do sensor de fluxo de água 
// (conta os sinais emitidos pelo sensor de efeito hall)
void rpmSensorFluxo()
{
  ciclosFluxo++;
}

// TODO: Implementar funcionalidade para gravar os dados localmente,
// quando uma conexão com a Internet ou com o equipamente receptor
// não estiver disponível, garantindo assim que os dados dos registros
// sejam salvos localmente para posterior reenvio

int main() {
    
    // Desliga os Leds da LPCXpresso4337
    ledR = 0;
    ledG = 0;
    ledB = 0;
    
    // Configura comunicação serial com o PC
    pc.baud(115200);
    
    pc.printf("\r\n    Embarcados Contest NXP - Aldiwa - Sistema de Controle Sustentavel - v0.2    ");
    pc.printf("\r\nhttp://contest.embarcados.com.br/projetos/aldiwa-sistema-de-controle-sustentavel");
    pc.printf("\r\n********************************************************************************\r\n");
    
    // Atacha o botão de modo de configuração à função de acionamento do modo de configuração
    botaoModoConfiguracao.rise(&acionamentoBotaoModoConfiguracao);

    // Atacha o botão de corte de fluxo à função de acionamento de corte do fluxo de água
    botaoCorteFluxo.rise(&acionamentoBotaoCorteFluxo);

    // Inicializa o Tempo de Uptime    
    diferencaTempoUptime = time(NULL);

    // Inicializa o Tempo para captura dos Dados de Temperatura e Umidade
    diferencaTempUmidade = time(NULL);

    // Inicializa o Tempo para checagem do Fluxo de Agua
    checagemFluxoAgua = time(NULL);

    while(1) {
        // Relogio de Tempo Real
        Rtc_Ds1307::Time_rtc relogio = {};
        rtc.getTime(relogio);

        // Obtem o Tempo de Uptime
        time_t segundosUptime = time(NULL);

        // A cada 60 segundos...
        if ((segundosUptime - diferencaTempoUptime)  > 60) {
            // Envia os dados para a serial
            pc.printf("#%d;%d;%04d-%02d-%02d %02d:%02d:%02d;%d#\r\n", 0, 0, relogio.year, relogio.mon, relogio.date, relogio.hour, relogio.min, relogio.sec, segundosUptime);
            // Atualiza o tempo de diferença
            diferencaTempoUptime = segundosUptime;
        }
        
        // Define o valor para 0, para poder contar as rotações
        ciclosFluxo = 0;

        // Atacha o sensor de fluxo de água à função de contagem das rotações
        sensorFluxo.rise(&rpmSensorFluxo);
        // Aguarda 1 segundo, para contar os pulsos do sensor
        wait_ms(1000);
        // Remove a interrupção
        sensorFluxo.rise(NULL);

        // Converte a contagem dos ciclos para L/min
        vazaoAgua = ciclosFluxo / 5.5;
        // Acumula a vazão para o calculo da média
        mediaVazaoAgua = mediaVazaoAgua + vazaoAgua;

        // Atualiza o contador de segundos decorridos
        segundosVazaoAgua++;
        
        // Caso possua vazão de água acumulada
        if ((vazaoAgua != 0) && (ativouFluxoAgua != 1)) {
            
            ativouFluxoAgua = 1;

            // Envia os dados para a serial
            pc.printf("#%d;%d;%04d-%02d-%02d %02d:%02d:%02d;%4.4f#\r\n", 1, 0, relogio.year, relogio.mon, relogio.date, relogio.hour, relogio.min, relogio.sec, vazaoAgua);
            
            
            // Imprime na serial o valor da vazão
            //pc.printf("%4.4f", vazaoAgua);
            
        }
         
        // A cada 5 segundos...
        if ((segundosUptime - checagemFluxoAgua)  > 5) {
            
            // Envia os dados para a serial
            pc.printf("#%d;%d;%04d-%02d-%02d %02d:%02d:%02d;%4.4f#\r\n", 1, 0, relogio.year, relogio.mon, relogio.date, relogio.hour, relogio.min, relogio.sec, vazaoAgua);
            
            // Atualiza o tempo de checagem
            checagemFluxoAgua = segundosUptime;

            if (vazaoAgua == 0) {
                ativouFluxoAgua = 0;
            }
        }
        
        // Caso tenha atingido o tempo de 1 minuto
        if (segundosVazaoAgua == 60) {
            // Obtem a média do minuto
            mediaVazaoAgua = mediaVazaoAgua / 60;
            
            // Se possui média do último minuto
            if (mediaVazaoAgua != 0) {
                // Envia os dados para a serial
                pc.printf("#%d;%d;%04d-%02d-%02d %02d:%02d:%02d;%4.4f#\r\n", 1, 1, relogio.year, relogio.mon, relogio.date, relogio.hour, relogio.min, relogio.sec, mediaVazaoAgua);
                // Imprime na serial o valor da média de vazão
                pc.printf("Media do Minuto: %4.4f L/min\n", mediaVazaoAgua);
                // Zera a media, para uma nova contagem
                mediaVazaoAgua = 0;
            }
            
            // Zera o contador (novo minuto)
            segundosVazaoAgua = 0;
        }
        
        // A cada 30 segundos...
        if ((segundosUptime - diferencaTempUmidade)  > 30) {

            // Lê os dados do sensor de Temperatura a Umidade (DHT11)
            erroDHT11 = sensorDht11.readData();
        
            // Se não possui erro na leitura
            if (erroDHT11 == 0) {

                // Registra a temperatura
                temperatura = sensorDht11.ReadTemperature(CELCIUS);

                // Controle da temperatura atual
                if (temperatura != temperaturaAtual) {
                    // Atualiza a temperatura atual
                    temperaturaAtual = temperatura;
                    // Envia os dados para a serial
                    pc.printf("#%d;%d;%04d-%02d-%02d %02d:%02d:%02d;%3.2f#\r\n", 2, 2, relogio.year, relogio.mon, relogio.date, relogio.hour, relogio.min, relogio.sec, temperaturaAtual);
                }
                            
                // Registra a umidade
                umidade = sensorDht11.ReadHumidity();
                
                // Controle da umidade atual
                if (umidade != umidadeAtual) {
                    // Atualiza a umidade atual
                    umidadeAtual = umidade;
                    // Envia os dados para a serial
                    pc.printf("#%d;%d;%04d-%02d-%02d %02d:%02d:%02d;%3.2f#\r\n", 3, 2, relogio.year, relogio.mon, relogio.date, relogio.hour, relogio.min, relogio.sec, umidadeAtual);
                }

            } else {
                // Imprime na serial o erro
                pc.printf("Ocorreu um erro ao ler os dados do sensor DHT11: %d\n", erroDHT11);
            }

            // Atualiza o tempo de diferença
            diferencaTempUmidade = segundosUptime;
        }
    }
}