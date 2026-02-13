# 🌦️ Station Météo – Projet Arduino / Grove

## 📌 Présentation du projet
Ce projet consiste à développer une station météorologique autonome intégrant :
- Capteurs environnementaux (température, humidité, pression…)
- Module GPS v1.2
- Horloge RTC v1.2
- Stockage sur carte SD
- Modes utilisateur (standard, configuration, maintenance, économie)
- Gestion d’erreurs et boutons poussoirs

---

## 🎯 Objectifs
- Acquérir et horodater des données environnementales
- Enregistrer les données sur carte SD
- Permettre la configuration via interface série
- Assurer un fonctionnement robuste et autonome

---

## 🧩 Architecture générale du système

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
