# Projeto IOT
Este projeto tem como objetivo criar um sistema de monitoramento utilizando uma câmera e beacons para:

* Detectar a presença de pessoas em um ambiente;
* Identificar a posição de objetos monitorados;
* Detectar possíveis furtos (quando um objeto sai da área permitida);
* Detectar a entrada de pessoas não autorizadas.

Sempre que um comportamento fora do padrão é identificado, um alerta é enviado automaticamente via **Telegram**, juntamente com a imagem capturada pela câmera.

## Estrutura do Projeto
Esse repositório se encontra organizado na seguinte forma:
Cada projeto é organizado da seguinte forma:

```text
├─ lib/
├─ src/
│    ├─ python/
│    │    └─ (códigos.py)
│    ├─ (códigos.cpp)
│    └─ certificados.h
├─ partitions.csv
├─ platformio.ini
├─ README.md
└─ requirements.txt
```

## Requisitos
Para que todos os códigos funcionem, sem nenhum erro, é necessário ter instalado as seguintes bibliotecas

### ESP32
As seguintes bibliotecas devem estar instaladas (na pasta `lib/` ou pela Arduino IDE):

| Biblioteca                    | Versão                     |
| ----------------------------- | -------------------------- |
| MQTT                          | 2.5.2                      |
| ArduinoJson                   | 7.4.3                      |
| lab_human_detection_inference | Disponível no [Edge Impulse](https://studio.edgeimpulse.com/public/338320/live) |

## Backend em Python

Crie um ambiente virtual:

```bash
python -m venv venv
```

Depois, instale todas as dependências:

```bash
pip install -r requirements.txt
```

# Arquivos de Configuração

## Certificados do ESP32

Crie um arquivo chamado:

```text
src/certificados.h
```

Este arquivo deve conter os certificados necessários para autenticação e conexão com Wi-Fi.

---

## Configuração do Python

Dentro da pasta:

```text
src/python/
```

crie um arquivo chamado:

```text
.env
```

com o seguinte conteúdo:

```env
username=SEU_USUARIO
password=SUA_SENHA
```

Essas credenciais serão utilizadas para autenticação no broker MQTT.

---

# Configurações Finais

Antes de compilar o projeto da câmera, configure no arquivo `camera.cpp` e `beacon.cpp`:

* Nome da rede Wi-Fi;
* Senha da rede Wi-Fi;
* Usuário do MQTT;
* Senha do MQTT.

---

## Node-Red

## Grafana

# Execução

## 1. Fazer upload aos ESP32

Utilize o PlatformIO para fazer o upload dos firmwares:

* **env:camera** → câmera
* **env:beacon** → trackers/beacons

---

## 2. Ativar o ambiente virtual

### Windows

```cmd
venv\Scripts\activate
```

### Linux

```bash
source venv/bin/activate
```

---

## 3. Iniciar o Node-RED

Execute o script disponibilizado na seção **Node-RED**.

---

## 4. Executar o backend Python

Com o ambiente virtual ativo, execute o backend responsável pela comunicação entre os dispositivos e o MQTT.

---

# Funcionamento

Após a configuração completa:

1. Os ESP32 iniciam a comunicação com o broker MQTT;
2. A câmera realiza a detecção de pessoas;
3. Os beacons monitoram a posição dos objetos;
4. O backend processa os dados recebidos;
5. Caso seja detectada uma situação de risco (intrusão ou possível furto), um alerta é enviado automaticamente pelo Telegram juntamente com a imagem capturada pela câmera.