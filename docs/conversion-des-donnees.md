# Station Météo – Projet S3E - Conversion des données 

## Programme 
Pour utiliser les données de la cartes SD, vous pouvez créer un Excel, l'enregistrer sous forma "xlsm", ensuite l'ouvrir, puis tapper la coommande CTRL + F11, ce qui ouvre l'éditeur de code, y créer un module et y mettre le programme suivant : 

```vb
'---------------------------------------------------------
' MACRO PRINCIPALE
'---------------------------------------------------------
Sub Traitement_Station_Meteo()

    Dim ws As Worksheet
    Dim filePath As String
    Dim fileNum As Integer
    Dim line As String
    Dim parts As Variant
    Dim row As Long
    Dim lastRow As Long
    Dim i As Long

    Set ws = ActiveSheet
    ws.Cells.Clear

    '---------------------------------------------------------
    ' 0) Sélection du fichier
    '---------------------------------------------------------
    filePath = Application.GetOpenFilename( _
                FileFilter:="Fichiers texte (*.txt;*.csv),*.txt;*.csv", _
                Title:="Sélectionnez le fichier de données")

    If filePath = "False" Then
        MsgBox "Aucun fichier sélectionné.", vbExclamation
        Exit Sub
    End If

    '---------------------------------------------------------
    ' 1) Import propre (ne touche pas aux dates)
    '---------------------------------------------------------
    fileNum = FreeFile
    Open filePath For Input As #fileNum

    row = 2 ' On commence à la ligne 2 (ligne 1 = en-têtes)

    Do Until EOF(fileNum)
        Line Input #fileNum, line

        ' Split direct sur les points-virgules
        parts = Split(line, ";")

        ' Remplir les colonnes
        For i = 0 To UBound(parts)
            ws.Cells(row, i + 1).Value = parts(i)
        Next i

        row = row + 1
    Loop

    Close #fileNum

    '---------------------------------------------------------
    ' 2) Ajouter les en-têtes
    '---------------------------------------------------------
    ws.Cells(1, 1).Value = "Temps"
    ws.Cells(1, 2).Value = "Température (°C)"
    ws.Cells(1, 3).Value = "Humidité (%)"
    ws.Cells(1, 4).Value = "Luminosité"
    ws.Cells(1, 5).Value = "Latitude"
    ws.Cells(1, 6).Value = "Longitude"

    '---------------------------------------------------------
    ' 3) Conversion automatique de la date RTC
    '---------------------------------------------------------
    lastRow = ws.Cells(ws.Rows.Count, 1).End(xlUp).Row

    For i = 2 To lastRow
        If IsDate(ws.Cells(i, 1).Value) Then
            ws.Cells(i, 1).Value = CDate(ws.Cells(i, 1).Value)
            ws.Cells(i, 1).NumberFormat = "dd/mm/yyyy hh:mm:ss"
        End If
    Next i

    '---------------------------------------------------------
    ' 4) Création des graphiques
    '---------------------------------------------------------
    Call CreerGraphique(ws, "Température (°C)", 2, lastRow, 300, 50)
    Call CreerGraphique(ws, "Humidité (%)", 3, lastRow, 300, 350)
    Call CreerGraphique(ws, "Luminosité", 4, lastRow, 300, 650)

    MsgBox "Traitement terminé !", vbInformation

End Sub


'---------------------------------------------------------
' SOUS-PROGRAMME : CRÉATION AUTOMATIQUE D’UN GRAPHIQUE
'---------------------------------------------------------
Sub CreerGraphique(ws As Worksheet, titre As String, colData As Long, lastRow As Long, posX As Long, posY As Long)

    Dim chartObj As ChartObject
    Dim rngX As Range, rngY As Range

    Set rngX = ws.Range(ws.Cells(2, 1), ws.Cells(lastRow, 1))
    Set rngY = ws.Range(ws.Cells(2, colData), ws.Cells(lastRow, colData))

    Set chartObj = ws.ChartObjects.Add(Left:=posX, Top:=posY, Width:=500, Height:=250)
    chartObj.Chart.ChartType = xlLine

    chartObj.Chart.SetSourceData Source:=Union(rngX, rngY)
    chartObj.Chart.HasTitle = True
    chartObj.Chart.ChartTitle.Text = titre
    chartObj.Chart.Axes(xlCategory).HasTitle = True
    chartObj.Chart.Axes(xlCategory).AxisTitle.Text = "Temps"
    chartObj.Chart.Axes(xlValue).HasTitle = True
    chartObj.Chart.Axes(xlValue).AxisTitle.Text = titre

End Sub
```

![Image manuel](../images/montage-complet.png)

Ensuite vous devrez créer un bouton et y lier la macro "Conversion_Donnees.xlsm!Module1.Traitement_Station_Meteo" et vous pourrez alors appuyer dessus, ce qui ouvre l'explorateur de fichier, sélectionnez le fichier "donnees" de la carte sd, ce qui importe les données et crée des graphiques, vous pourrez avoir alors des graphiques. 

Des exemples de données et de grphiques sont disponible ci-dessous en exemples : 

## Visualisation des données

### Luminosité
![graph-luminosite](../images/luminosite.png)

### Humidité
![graph-humidite](../images/humidite.png)

### Température
![graph-temperature](../images/temperature.png)

### Données d’exemple
![donnees-exemple](../images/donnees-exemples.png)

