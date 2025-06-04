# Anleitung ESP Raumsensoren

## Server

Alle notwendigen Services laufen auf einem Raspberry Pi 5, die IP-Adresse im WLAN IoT-WLAN ist 172.20.200.60.

Die Services werden über Docker verwaltet. Die Docker können über die Weboberfläche Portainer unter dem Port 9443 verwaltet werden.

| Service Name | Port | Aufgabe | Logins |
|--------------|------|---------|--------|
| Raspberry Pi Server | SSH Port: 22 | Server | pi, raspberry |
| Portainer | 9443 | Docker mit Oberfläche verwalten | admin, adminadminadmin |
| Apache | 80 | Setllt die Website zur Verfügung die auf dem Monitor angezeigt werden soll | |
| Grafana | 3000 | Stellt die grafische Darstellung der Daten zur Verfügung | admin, admin |
| MariaDB | 3306 | Stellt die Datenbank in die unsere Sensoren und Daten gespeichert werden | root, root |
| Mosquitto | 1883 | MQTT Broker um die Daten zu verschicken | admin, admin |
| Node-RED | 1880 | Empfängt die Daten von MQTT und schreibt sie in die MariaDB Datenbank | |
| PHPAdmin | 8080 | Weboberfläche um MariaDB zu verwalten | root, root |

## Clients

Der Code für die ESP Clients ist in einem GitHub Repository zu finden: [github.com/wyattwas/room-climate-sensor](https://github.com/wyattwas/room-climate-sensor)
