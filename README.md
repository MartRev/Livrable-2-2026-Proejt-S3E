# 🌦️ Station Météo – Projet Arduino / Grove

## Présentation du projet
L'Agence Internationale pour la Vigilance Météorologique (AIVM) se lance dans un projet ambitieux : déployer dans les océans des navires de surveillance équipés de stations météo embarquées chargées de mesurer les paramètres influant sur la formation de cyclones ou autres catastrophes naturelles.

Un grand nombre de sociétés utilisant des transports navals ont accepté d'équiper leurs bateaux avec ces stations embarquées. En revanche, ces dernières devront être simples et efficaces et pilotables par un des membres de l'équipage (une documentation technique utilisateur sera mise à disposition).



---

## Objectifs
- Acquérir et horodater des données environnementales
- Enregistrer les données sur carte SD
- Permettre la configuration via interface série
- Assurer un fonctionnement robuste et autonome

---

## Architecture générale du système

```mermaid
flowchart LR
    %% Acteurs
    Utilisateur((Utilisateur))
    Technicien((Technicien))
    Capteurs((Capteurs))
    SD((Carte SD))
    RTC((RTC))
    GPS((GPS))

    %% Système
    subgraph Systeme["Systeme de la station météorologique"]
        UC1[Demarrer en mode standard]
        UC2[Acquerir les donnees des capteurs]
        UC3[Horodater les donnees]
        UC4[Enregistrer les donnees sur carte SD]

        UC5[Demarrer en mode configuration]
        UC6[Configurer les parametres du systeme]
        UC7[Configurer date et heure RTC]

        UC8[Acceder au mode maintenance]
        UC9[Consulter les donnees via interface serie]
        UC10[Remplacer la carte SD en securite]

        UC11[Activer le mode economique]
        UC12[Reduire la frequence d acquisition et désactiver certain capteurs]

        UC13[Detecter et signaler les erreurs]
        UC14[Action sur les boutons poussoirs]
    end

    %% Liens Utilisateur
    Utilisateur --> UC14

    %% Liens Technicien
    Technicien --> UC6
    Technicien --> UC7
    Technicien --> UC9
    Technicien --> UC10
    Technicien --> UC14

    %% Liens Capteurs et modules
    Capteurs --> UC2
    RTC --> UC3
    GPS --> UC2
    SD --> UC4
    SD --> UC10

    %% Relations entre cas d'utilisation
    UC1 --> UC2
    UC2 --> UC3
    UC3 --> UC4

    UC5 --> UC6
    UC5 --> UC7

    UC8 --> UC9
    UC8 --> UC10

    UC11 --> UC12

    %% Erreurs (cas transversal)
    UC2 --> UC13
    UC3 --> UC13
    UC4 --> UC13

    %% Lien entre les modes et les boutons poussoirs
    UC14 --> UC1
    UC14 --> UC5
    UC14 --> UC8
    UC14 --> UC11
```

```mermaid
flowchart TD

    Start([Démarrage du système])

    Start --> D0{Bouton rouge pressé au démarrage ?}
    D0 -- Oui --> Configuration
    D0 -- Non --> Standard

    Standard["Mode STANDARD activé"]

    Standard --> D1{Bouton vert 5s ?}
    D1 -- Oui --> Economique
    D1 -- Non --> D2{Bouton rouge 5s ?}
    D2 -- Oui --> Maintenance_S
    D2 -- Non --> Standard

    Configuration["Mode CONFIGURATION activé (Acquisition désactivée)"]

    Configuration --> D3{30 min sans activité ?}
    D3 -- Oui --> Standard
    D3 -- Non --> Configuration

    Economique["Mode ECONOMIQUE activé (Capteurs partiellement désactivés)"]

    Economique --> D4{Bouton rouge 5s ?}
    D4 -- Oui --> Standard
    D4 -- Non --> D5{Bouton rouge pressé ?}
    D5 -- Oui --> Maintenance_E
    D5 -- Non --> Economique

    Maintenance_S["Mode MAINTENANCE activé (depuis STANDARD)"]
    Maintenance_E["Mode MAINTENANCE activé (depuis ECONOMIQUE)"]

    Maintenance_S --> D6{Bouton rouge 5s ?}
    D6 -- Oui --> Standard
    D6 -- Non --> Maintenance_S

    Maintenance_E --> D7{Bouton rouge 5s ?}
    D7 -- Oui --> Economique
    D7 -- Non --> Maintenance_E
```
##Mode de fonctionnement : Changement de LED 
```mermaid
flowchart TD

    Start([Système en fonctionnement])

    Start --> Mode{Mode actif ?}

    Mode -->|Standard| LED_Standard[LED verte continue]
    Mode -->|Configuration| LED_Config[LED jaune continue]
    Mode -->|Economique| LED_Eco[LED bleue continue]
    Mode -->|Maintenance| LED_Maint[LED orange continue]

    Start --> Erreur{Erreur détectée ?}

    Erreur -->|RTC| LED_RTC[LED rouge/bleue clignotante 1Hz]
    Erreur -->|GPS| LED_GPS[LED rouge/jaune clignotante 1Hz]
    Erreur -->|Capteur| LED_CAPT[LED rouge/verte clignotante 1Hz]
    Erreur -->|Données incohérentes| LED_INCOH[LED rouge/verte vert plus long]
    Erreur -->|SD pleine| LED_SD_FULL[LED rouge/blanche clignotante 1Hz]
    Erreur -->|Erreur SD| LED_SD_ERR[LED rouge/blanche blanc plus long]
```

