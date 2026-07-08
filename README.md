# Projeto IOT
O nosso projeto visa criar um sistema de câmera para detectar a presença de pessoas e o posicionamento de objetos com um beacon. Dessa forma, conseguimos identificar se há um possível furto (caso o objeto sai da área especificada) ou caso alguém não autorizado entre no local.  
Caso seja detectado algum desses comportamentos fora do padrão, um alerta é enviado via Telegram.

## Estrutura do Projeto
Esse repositório se encontra organizado na seguinte forma:
Cada projeto é organizado da seguinte forma:

```text
├─ lib/
├─ src/
    ├─ python/
    ├─ (códigos.cpp)
├─ partitions.csv
├─ platformio.ini
├─ README.md
├─ requirements.txt
```

## Requisitos
Para que todos os códigos funcionem, sem nenhum erro, é necessário ter instalado as seguintes bibliotecas

### Bibliotecas para o ESP32
Na pasta [lib](/lib/), ou através do arduinoIDE, deverá conter as seguintes bibliotecas:
- MQTT -> Versão 2.5.2
- ArduinoJson -> Versão 7.4.3
- lab_human_detection_inference -> Disponível através do link [edgeImpulse](https://studio.edgeimpulse.com/public/338320/live)

### Bibliotecas para Python
Para baixar todas as bibliotecas necessárias para que o backend em python funcione, basta criar um ambiente virtual do pyhton:

```cmd
python -m venv venv
```

E depois baixar as bibliotecas listadas em [requirements.txt](/requirements.txt) através do comando:

```python
pip install -r requirements.txt
```

### Códigos extras
Além disso, também será necesssário colocar um código na pasta [src](/src/) com o nome `certificados.h`, com os certificados para que seja possível se conectar tanto ao Telegram quanto ao WiFi

### Ajustes
Por fim, após todos esses ajustes, basta adicionar a senha e o nome da rede WiFi, além de adicionar um login e senha para o MQTT no código [camera.cpp](/src/camera.cpp)  
Para o pyhton, basta criar um `.env` dentro da pasta [/python](/src/python/) e colocar as informações para que seja possível logar no MQTT:

```pyhton
username = (usuário)
password = (senha)
```

## Node-Red

## Grafana

## Funcionamento
Após configurar tudo listado a cima, basta fazer o upload do código da câmera, o qual pode ser feito dentro do ambiente `env:camera` do PlatformIO, e o mesmo pode ser feito para o código para os trackers, com o ambiente `env:beacon` 