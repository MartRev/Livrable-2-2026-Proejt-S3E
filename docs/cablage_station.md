# Station Météo – Projet S3E - Montage en réel de la station 

## Montage réel
Pour réaliser le montage de la station, vous aurez besoin de cette liste de composant et d'outils exterieur à arduino pour un fonctionnement total.

## Matériel utilisé

Le projet utilise les composants suivants :

- **Carte Arduino Uno R3 + câble d'alimentation Arduino**  
  - Le programme est stocké directement sur la carte.

- **Base Shield Grove Arduino**  
  - Sélecteur configuré sur **5V**

- **Dual Button**
  - Connecté sur **D6**

- **Module RTC (Real Time Clock)**
  - Communication **I2C**

- **Module GPS**
  - Connexions :
    - **TX** → Digital **10**
    - **RX** → Digital **13**
    - **VCC** → **5V**
    - **GND** → **GND**

- **Capteur de température et d’humidité**
  - Connecté sur **D2**

- **Capteur de luminosité**
  - Connecté sur **A0**

- **LED RGB**
  - Entrée **IN** connectée sur **D4**

- **Écran LCD**
  - Communication **I2C**

- **Adaptateur Micro SD**
  - Connexions :
    - **CS** → Digital **10**
    - **SCK** → Digital **13**
    - **MOSI** → Digital **11**
    - **MISO** → Digital **12**
    - **VCC** → **5V**
    - **GND** → **GND**
  
- **Carte Micro SD (32 Go)**

- **Adaptateur Micro SD vers USB**
  - Utilisé pour lire la carte SD depuis un ordinateur
 
- **BreadBoard pour l'alimentation GPS et Adaptateur Micro SD**  

- **12 câbles mâles-femelles pour connecter le GPS, l'Adaptateur Micro SD et leur alimentation**

- **6 câbles femelles-femelles universel Grove à 4 broches pour les autres capteurs**  

## Schéma de montage

Le montage se fait ensuite en commançant par la carte

## Schéma de montage
Vous pourrez alors réaliser le montage qui va suivre, en premier il faudra fixer le shield sur l'arduino comme ceci : 

<p align="center">
  <img src="../images/montage%20arduino+%20shield%20grove.png" alt="Montage Arduino + Shield Grove" width="800">
</p>
Une fois fait, vous pouvez brancher l'alimentation de l'arduino, mettre le sélecteur de puissnace du shield sur "5V", si c'est sur "3.3V", le circuit ne sera pas focntionnel avec le programme fournit, et ensuite faire le montage complet.
![Image manuel](../images/montage-complet.png)


