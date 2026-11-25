# Sistema de Irrigação Automatizada com ESP32, IoT e Deep Sleep (Trilha T1)

Projeto desenvolvido para o Processo Seletivo Simplificado (LAISA / Agrifuture Hub / LIAMA — UNIVASF), implementando o monitoramento de umidade do solo, controle de irrigação por histerese configurável, economia de energia via *Deep Sleep* e telemetria em tempo real via protocolo MQTT.

## Links da Entrega
- **Simulação no Wokwi:** https://wokwi.com/projects/481676587082261505
- **Vídeo Demonstrativo (até 2 min):** [Cole aqui o link do vídeo no YouTube não listado ou Google Drive]

---

## Arquitetura e Funcionamento

1. **Sensoriamento (Entrada Analógica):** Um potenciômetro conectado ao pino `GPIO 34` (canal ADC1 de 12 bits) simula a leitura de tensão de um sensor capacitivo de umidade do solo, convertendo o sinal bruto (`0` a `4095`) para escala percentual (`0.0%` a `100.0%`).
2. **Controle por Histerese Configurável:** O acionamento da bomba (simulada por um módulo relé no pino `GPIO 26`) opera com dois limiares distintos para evitar chaveamentos intermitentes (*chattering*) provocados por oscilações de leitura:
   - **Limiar de acionamento (`umidadeLiga`):** `< 35.0%` (liga a bomba).
   - **Limiar de desligamento (`umidadeDesliga`):** `> 60.0%` (desliga a bomba).
3. **Eficiência Energética com Deep Sleep (Extra):** Após realizar a leitura, atualizar o estado da bomba e transmitir os dados, o microcontrolador entra em hibernação profunda (`esp_deep_sleep_start()`) para reduzir o consumo energético em aplicações alimentadas por bateria e painel solar no campo:
   - **Persistência em Memória RTC (`RTC_DATA_ATTR`):** O estado da bomba (`estadoBomba`), o contador de ciclos e os limiares configurados permanecem salvos na memória RTC durante o *Deep Sleep*.
   - **Retenção de Pino (`gpio_hold_en`):** O estado lógico do pino `GPIO 26` é travado antes da hibernação (`gpio_deep_sleep_hold_en()`), garantindo que a irrigação continue ativa ou desligada mesmo com o núcleo do ESP32 em repouso.
4. **Comunicação IoT (MQTT + JSON):** O ESP32 publica pacotes JSON no broker público `broker.hivemq.com` (porta `1883`) e consulta um tópico de configuração que permite ajustar os limiares de histerese remotamente sem regravar o firmware.

### Tópicos MQTT Utilizados
- **Publicação (Telemetria):** `univasf/laisa/igor/telemetria`
  ```json
  {
    "ciclo": 1,
    "umidade_solo": 42.5,
    "bomba_ligada": false,
    "limiar_liga": 35.0,
    "limiar_desliga": 60.0,
    "deep_sleep_s": 10
  }
  ```
- **Assinatura (Configuração remota):** `univasf/laisa/igor/config`
  ```json
  {
    "liga": 40.0,
    "desliga": 70.0
  }
  ```

---

## Estrutura do Repositório
- `sketch.ino`: Código-fonte do firmware em C++ (framework Arduino para ESP32).
- `diagram.json`: Arquivo de definição do circuito e conexões dos pinos no simulador Wokwi.
- `README.md`: Documentação técnica do projeto.

---

## Declaração de Uso de Inteligência Artificial
Em conformidade com o item 4.3 do Edital, declaro que ferramentas de Inteligência Artificial (Google Gemini) foram utilizadas como apoio na estruturação do firmware (comunicação MQTT/JSON e gerenciamento de memória RTC no modo *Deep Sleep*) e na revisão da documentação técnica. Toda a montagem do circuito no simulador, os testes de validação da histerese e da hibernação, a depuração da comunicação com o broker e a gravação demonstrativa foram executados e verificados por mim.
