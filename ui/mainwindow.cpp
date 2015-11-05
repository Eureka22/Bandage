﻿//Copyright 2015 Ryan Wick

//This file is part of Bandage

//Bandage is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//Bandage is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

//You should have received a copy of the GNU General Public License
//along with Bandage.  If not, see <http://www.gnu.org/licenses/>.


#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QLatin1String>
#include <QTextStream>
#include <QLocale>
#include <QRegExp>
#include <ogdf/energybased/FMMMLayout.h>
#include <cmath>
#include "../program/settings.h"
#include <QClipboard>
#include <QTransform>
#include <QFontDialog>
#include <QColorDialog>
#include <algorithm>
#include <QFile>
#include <QTextStream>
#include <QScrollBar>
#include "settingsdialog.h"
#include <stdlib.h>
#include <time.h>
#include <QProgressDialog>
#include <QThread>
#include "../program/graphlayoutworker.h"
#include <QRegExp>
#include <QMessageBox>
#include <QInputDialog>
#include <QShortcut>
#include "aboutdialog.h"
#include <QMainWindow>
#include "blastsearchdialog.h"
#include "../graph/assemblygraph.h"
#include "mygraphicsview.h"
#include "graphicsviewzoom.h"
#include "mygraphicsscene.h"
#include "../blast/blastsearch.h"
#include "../graph/debruijnnode.h"
#include "../graph/debruijnedge.h"
#include "../graph/graphicsitemnode.h"
#include "../graph/graphicsitemedge.h"
#include "myprogressdialog.h"
#include <limits>
#include <QDesktopServices>
#include <QSvgGenerator>
#include "../graph/path.h"
#include "pathspecifydialog.h"
#include "../program/memory.h"
#include <QDebug>
#include "program/globals.h"
#include "graph/barcodesetting.h"

MainWindow::MainWindow(QString fileToLoadOnStartup, bool drawGraphAfterLoad) :
    QMainWindow(0),
    ui(new Ui::MainWindow), m_layoutThread(0), m_imageFilter("PNG (*.png)"),
    m_fileToLoadOnStartup(fileToLoadOnStartup), m_drawGraphAfterLoad(drawGraphAfterLoad),
    m_uiState(NO_GRAPH_LOADED), m_blastSearchDialog(0), m_alreadyShown(false)
{
    ui->setupUi(this);

    QApplication::setWindowIcon(QIcon(QPixmap(":/icons/icon.png")));
    ui->graphicsViewWidget->layout()->addWidget(g_graphicsView);

    srand(time(NULL));

    //Make a temp directory to hold the BLAST files.
    //Since Bandage is running in GUI mode, we make it in the system's
    //temp area - out of the way for the user.
    g_blastSearch->m_tempDirectory = QDir::tempPath() + "/bandage_temp-" + QString::number(QCoreApplication::applicationPid()) + "/";
    if (!QDir().mkdir(g_blastSearch->m_tempDirectory))
    {
        QMessageBox::warning(this, "Error", "A temporary directory could not be created.  BLAST search functionality will not be available");
        return;
    }
    else
        g_blastSearch->m_blastQueries.createTempQueryFiles();

    m_previousZoomSpinBoxValue = ui->zoomSpinBox->value();
    ui->zoomSpinBox->setMinimum(g_settings->minZoom * 100.0);
    ui->zoomSpinBox->setMaximum(g_settings->maxZoom * 100.0);

    //The normal height of the QPlainTextEdit objects is a bit much,
    //so fix them at a smaller height.
    ui->selectedNodesTextEdit->setFixedHeight(ui->selectedNodesTextEdit->sizeHint().height() / 2.5);
    ui->selectedEdgesTextEdit->setFixedHeight(ui->selectedEdgesTextEdit->sizeHint().height() / 2.5);

    setUiState(NO_GRAPH_LOADED);

    m_graphicsViewZoom = new GraphicsViewZoom(g_graphicsView);
    g_graphicsView->m_zoom = m_graphicsViewZoom;

    m_scene = new MyGraphicsScene(this);
    g_graphicsView->setScene(m_scene);

    setInfoTexts();

    //Nothing is selected yet, so this will hide the appropriate labels.
    selectionChanged();

    //The user may have specified settings on the command line, so it is now
    //necessary to update the UI to match these settings.
    setWidgetsFromSettings();
    setTextDisplaySettings();

    graphScopeChanged();
    switchColourScheme();

    connect(ui->drawGraphButton, SIGNAL(clicked()), this, SLOT(drawGraph()));
    connect(ui->actionLoad_graph, SIGNAL(triggered()), this, SLOT(loadGraph()));
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->graphScopeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(graphScopeChanged()));
    connect(ui->zoomSpinBox, SIGNAL(valueChanged(double)), this, SLOT(zoomSpinBoxChanged()));
    connect(m_graphicsViewZoom, SIGNAL(zoomed()), this, SLOT(zoomedWithMouseWheel()));
    connect(ui->actionCopy_selected_node_sequences_to_clipboard, SIGNAL(triggered()), this, SLOT(copySelectedSequencesToClipboardActionTriggered()));
    connect(ui->actionSave_selected_node_sequences_to_FASTA, SIGNAL(triggered()), this, SLOT(saveSelectedSequencesToFileActionTriggered()));
    connect(ui->actionCopy_selected_node_path_to_clipboard, SIGNAL(triggered(bool)), this, SLOT(copySelectedPathToClipboard()));
    connect(ui->actionSave_selected_node_path_to_FASTA, SIGNAL(triggered(bool)), this, SLOT(saveSelectedPathToFile()));
    connect(ui->coloursComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(switchColourScheme()));
    connect(ui->actionSave_image_current_view, SIGNAL(triggered()), this, SLOT(saveImageCurrentView()));
    connect(ui->actionSave_image_entire_scene, SIGNAL(triggered()), this, SLOT(saveImageEntireScene()));
    connect(ui->nodeCustomLabelsCheckBox, SIGNAL(toggled(bool)), this, SLOT(setTextDisplaySettings()));
    connect(ui->nodeNamesCheckBox, SIGNAL(toggled(bool)), this, SLOT(setTextDisplaySettings()));
    connect(ui->nodeLengthsCheckBox, SIGNAL(toggled(bool)), this, SLOT(setTextDisplaySettings()));
    connect(ui->nodeReadDepthCheckBox, SIGNAL(toggled(bool)), this, SLOT(setTextDisplaySettings()));
    connect(ui->blastHitsCheckBox, SIGNAL(toggled(bool)), this, SLOT(setTextDisplaySettings()));
    connect(ui->textOutlineCheckBox, SIGNAL(toggled(bool)), this, SLOT(setTextDisplaySettings()));
    connect(ui->fontButton, SIGNAL(clicked()), this, SLOT(fontButtonPressed()));
    connect(ui->setNodeCustomColourButton, SIGNAL(clicked()), this, SLOT(setNodeCustomColour()));
    connect(ui->setNodeCustomLabelButton, SIGNAL(clicked()), this, SLOT(setNodeCustomLabel()));
    connect(ui->removeNodeButton, SIGNAL(clicked()), this, SLOT(removeNodes()));
    connect(ui->actionSettings, SIGNAL(triggered()), this, SLOT(openSettingsDialog()));
    connect(ui->selectNodesButton, SIGNAL(clicked()), this, SLOT(selectUserSpecifiedNodes()));
    connect(ui->selectionSearchNodesLineEdit, SIGNAL(returnPressed()), this, SLOT(selectUserSpecifiedNodes()));
    connect(ui->setBarcodeButton, SIGNAL(clicked()), this, SLOT(selectUserSpecifiedBarcodes()));
    connect(ui->refreshDispButton, SIGNAL(clicked()), this, SLOT(refreshDisplay()));

    connect(ui->barcodeInput, SIGNAL(returnPressed()), this, SLOT(selectUserSpecifiedBarcodes()));
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(openAboutDialog()));
    connect(ui->blastSearchButton, SIGNAL(clicked()), this, SLOT(openBlastSearchDialog()));
    connect(ui->blastQueryComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(blastQueryChanged()));
    connect(ui->actionControls_panel, SIGNAL(toggled(bool)), this, SLOT(showHidePanels()));
    connect(ui->actionSelection_panel, SIGNAL(toggled(bool)), this, SLOT(showHidePanels()));
    connect(ui->contiguityButton, SIGNAL(clicked()), this, SLOT(determineContiguityFromSelectedNode()));
    connect(ui->actionBring_selected_nodes_to_front, SIGNAL(triggered()), this, SLOT(bringSelectedNodesToFront()));
    connect(ui->actionSelect_nodes_with_BLAST_hits, SIGNAL(triggered()), this, SLOT(selectNodesWithBlastHits()));
    connect(ui->actionSelect_all, SIGNAL(triggered()), this, SLOT(selectAll()));
    connect(ui->actionSelect_none, SIGNAL(triggered()), this, SLOT(selectNone()));
    connect(ui->actionInvert_selection, SIGNAL(triggered()), this, SLOT(invertSelection()));
    connect(ui->actionZoom_to_selection, SIGNAL(triggered()), this, SLOT(zoomToSelection()));
    connect(ui->actionSelect_contiguous_nodes, SIGNAL(triggered()), this, SLOT(selectContiguous()));
    connect(ui->actionSelect_possibly_contiguous_nodes, SIGNAL(triggered()), this, SLOT(selectMaybeContiguous()));
    connect(ui->actionSelect_not_contiguous_nodes, SIGNAL(triggered()), this, SLOT(selectNotContiguous()));
    connect(ui->actionBandage_website, SIGNAL(triggered()), this, SLOT(openBandageUrl()));
    connect(ui->nodeDistanceSpinBox, SIGNAL(valueChanged(int)), this, SLOT(nodeDistanceChanged()));
    connect(ui->startingNodesExactMatchRadioButton, SIGNAL(toggled(bool)), this, SLOT(startingNodesExactMatchChanged()));
    connect(ui->actionSpecify_exact_path_for_copy_save, SIGNAL(triggered()), this, SLOT(openPathSpecifyDialog()));
    connect(ui->nodeWidthSpinBox, SIGNAL(valueChanged(double)), this, SLOT(nodeWidthChanged()));
    connect(g_graphicsView, SIGNAL(copySelectedSequencesToClipboard()), this, SLOT(copySelectedSequencesToClipboard()));
    connect(g_graphicsView, SIGNAL(saveSelectedSequencesToFile()), this, SLOT(saveSelectedSequencesToFile()));
    connect(ui->barcodeTable->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), SLOT(setBarcodeSelectionNode(const QItemSelection &, const QItemSelection &)));

    connect(this, SIGNAL(windowLoaded()), this, SLOT(afterMainWindowShow()), Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));

    QShortcut *colourShortcut = new QShortcut(QKeySequence("Ctrl+O"), this);
    connect(colourShortcut, SIGNAL(activated()), this, SLOT(setNodeCustomColour()));
    QShortcut *labelShortcut = new QShortcut(QKeySequence("Ctrl+L"), this);
    connect(labelShortcut, SIGNAL(activated()), this, SLOT(setNodeCustomLabel()));
    QShortcut *removeShortcut1 = new QShortcut(QKeySequence("Backspace"), this);
    connect(removeShortcut1, SIGNAL(activated()), this, SLOT(removeNodes()));
    QShortcut *removeShortcut2 = new QShortcut(QKeySequence("Delete"), this);
    connect(removeShortcut2, SIGNAL(activated()), this, SLOT(removeNodes()));

    //On the Mac, the shortcut keys will be using the command button, not the control button
    //so change the tooltips to reflect this.
#ifdef Q_OS_MAC
    QString command(QChar(0x2318));
    ui->setNodeCustomColourButton->setToolTip(command + 'O');
    ui->setNodeCustomLabelButton->setToolTip(command + 'L');
#endif
}


//This function runs after the MainWindow has been shown.  This code is not
//included in the contructor because it can perform a BLAST search, which
//will fill the BLAST query combo box and screw up widget sizes.
void MainWindow::afterMainWindowShow()
{
    if (m_alreadyShown)
        return;

    //If the user passed a filename as a command line argument, try to open it now.
    if (m_fileToLoadOnStartup != "")
        loadGraph(m_fileToLoadOnStartup);

    //If a BLAST query filename is present, do the BLAST search now automatically.
    if (g_settings->blastQueryFilename != "")
    {
        BlastSearchDialog blastSearchDialog(this, g_settings->blastQueryFilename);
        setupBlastQueryComboBox();
    }

    //If the draw option was used and the graph appears to have loaded (i.e. there
    //is at least one node), then draw the graph.
    if (m_fileToLoadOnStartup != "" && m_drawGraphAfterLoad && g_assemblyGraph->m_deBruijnGraphNodes.size() > 0)
        drawGraph();

    m_alreadyShown = true;
}

MainWindow::~MainWindow()
{
    cleanUp();
    delete m_graphicsViewZoom;
    delete ui;

    if (g_blastSearch->m_tempDirectory != "" &&
            QDir(g_blastSearch->m_tempDirectory).exists() &&
            QDir(g_blastSearch->m_tempDirectory).dirName().contains("bandage_temp"))
        QDir(g_blastSearch->m_tempDirectory).removeRecursively();
}



void MainWindow::cleanUp()
{
    ui->blastQueryComboBox->clear();
    ui->blastQueryComboBox->addItem("none");

    g_blastSearch->cleanUp();
    g_assemblyGraph->cleanUp();
    setWindowTitle("Bandage");

    g_memory->userSpecifiedPath = Path();
    g_memory->userSpecifiedPathString = "";
    g_memory->userSpecifiedPathCircular = false;

    if (m_blastSearchDialog != 0)
    {
        delete m_blastSearchDialog;
        m_blastSearchDialog = 0;
    }
}


void MainWindow::loadGraph(QString fullFileName)
{
    QString selectedFilter = "Any supported graph (*)";
    if (fullFileName == "")
        fullFileName = QFileDialog::getOpenFileName(this, "Load graph", g_memory->rememberedPath,
                                                    "Any supported graph (*);;LastGraph (*LastGraph*);;FASTG (*.fastg);;FASTG with barcodes(*.fastgbc);;GFA (*.gfa);;Trinity.fasta (*.fasta)",
                                                    &selectedFilter);

    if (fullFileName != "") //User did not hit cancel
    {
        GraphFileType detectedFileType = g_assemblyGraph->getGraphFileTypeFromFile(fullFileName);

        GraphFileType selectedFileType = ANY_FILE_TYPE;
        if (selectedFilter == "LastGraph (*LastGraph*)")
            selectedFileType = LAST_GRAPH;
        else if (selectedFilter == "FASTGBC (*.fastgbc)")
            selectedFileType = FASTG_BC;
        else if (selectedFilter == "FASTG (*.fastg)")
            selectedFileType = FASTG;
        else if (selectedFilter == "GFA (*.gfa)")
            selectedFileType = GFA;
        else if (selectedFilter == "Trinity.fasta (*.fasta)")
            selectedFileType = TRINITY;

        if (selectedFileType == ANY_FILE_TYPE)
        {
            //If the user chose any file type but it can't be determined, show an error and quit.
            if (detectedFileType == UNKNOWN_FILE_TYPE)
            {
                QMessageBox::warning(this, "Graph format not recognised", "Cannot load file. The selected file's format was not recognised as any supported graph type.");
                return;
            }

            //If the user chose any file type and it can be determined, then use that type.
            else
                selectedFileType = detectedFileType;
        }

        //If there is a discrepancy between the selected and detected file types, make sure the
        //user wants to continue.
        if (selectedFileType != detectedFileType)
        {
            QString graphFileTypeString = convertGraphFileTypeToString(selectedFileType);
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, graphFileTypeString + " file?",
                                          "This file does not appear to be a " + graphFileTypeString + " file."
                                          "\nDo you want to load it as a " + graphFileTypeString + " file anyway?",
                                          QMessageBox::Yes|QMessageBox::No);
            if (reply == QMessageBox::No)
                return;
        }
        //qDebug() << selectedFileType;
        loadGraph2(selectedFileType, fullFileName);
    }
}


void MainWindow::loadGraph2(GraphFileType graphFileType, QString fullFileName)
{
    resetScene();
    cleanUp();
    ui->selectionSearchNodesLineEdit->clear();

    try
    {
        MyProgressDialog progress(this, "Loading " + convertGraphFileTypeToString(graphFileType) + " file...", false);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();

        if (graphFileType == LAST_GRAPH)
            g_assemblyGraph->buildDeBruijnGraphFromLastGraph(fullFileName);
        else if (graphFileType == FASTG)
            g_assemblyGraph->buildDeBruijnGraphFromFastg(fullFileName);
        else if (graphFileType == FASTG_BC)
            g_assemblyGraph->buildDeBruijnGraphFromFastgBC(fullFileName, fullFileName+".barcode");
        else if (graphFileType == GFA)
            g_assemblyGraph->buildDeBruijnGraphFromGfa(fullFileName);
        else if (graphFileType == TRINITY)
            g_assemblyGraph->buildDeBruijnGraphFromTrinityFasta(fullFileName);

        setUiState(GRAPH_LOADED);
        setWindowTitle("Bandage - " + fullFileName);

        g_assemblyGraph->determineGraphInfo();
        displayGraphDetails();
        g_memory->rememberedPath = QFileInfo(fullFileName).absolutePath();
        g_memory->clearGraphSpecificMemory();
    }

    catch (...)
    {
        QString errorTitle = "Error loading " + convertGraphFileTypeToString(graphFileType);
        QString errorMessage = "There was an error when attempting to load:\n"
                               + fullFileName + "\n\n"
                               "Please verify that this file has the correct format.";
        QMessageBox::warning(this, errorTitle, errorMessage);
        resetScene();
        cleanUp();
        clearGraphDetails();
        setUiState(NO_GRAPH_LOADED);
    }
}



void MainWindow::displayGraphDetails()
{
    ui->nodeCountLabel->setText(formatIntForDisplay(g_assemblyGraph->m_nodeCount));
    ui->edgeCountLabel->setText(formatIntForDisplay(g_assemblyGraph->m_edgeCount));
    ui->totalLengthLabel->setText(formatIntForDisplay(g_assemblyGraph->m_totalLength));
}
void MainWindow::clearGraphDetails()
{
    ui->nodeCountLabel->setText("0");
    ui->edgeCountLabel->setText("0");
    ui->totalLengthLabel->setText("0");
}


void MainWindow::selectionChanged()
{
    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();
    std::vector<DeBruijnEdge *> selectedEdges = m_scene->getSelectedEdges();

    if (selectedNodes.size() == 0)
    {
        ui->selectedNodesTextEdit->setPlainText("");
        setSelectedNodesWidgetsVisibility(false);
    }

    else //One or more nodes selected
    {
        setSelectedNodesWidgetsVisibility(true);

        int selectedNodeCount;
        QString selectedNodeCountText;
        QString selectedNodeListText;
        QString selectedNodeLengthText;

        getSelectedNodeInfo(selectedNodeCount, selectedNodeCountText, selectedNodeListText, selectedNodeLengthText);

        if (selectedNodeCount == 1)
        {
            ui->selectedNodesTitleLabel->setText("Selected node");
            ui->removeNodeButton->setText("Remove node");
            ui->selectedNodesLengthLabel->setText("Length: " + selectedNodeLengthText);
        }
        else
        {
            ui->selectedNodesTitleLabel->setText("Selected nodes (" + selectedNodeCountText + ")");
            ui->removeNodeButton->setText("Remove nodes");
            ui->selectedNodesLengthLabel->setText("Total length: " + selectedNodeLengthText);
        }

        ui->selectedNodesTextEdit->setPlainText(selectedNodeListText);
    }


    if (selectedEdges.size() == 0)
    {
        ui->selectedEdgesTextEdit->setPlainText("");
        setSelectedEdgesWidgetsVisibility(false);
    }

    else //One or more edges selected
    {
        setSelectedEdgesWidgetsVisibility(true);
        if (selectedEdges.size() == 1)
            ui->selectedEdgesTitleLabel->setText("Selected edge");
        else
            ui->selectedEdgesTitleLabel->setText("Selected edges (" + formatIntForDisplay(int(selectedEdges.size())) + ")");

        ui->selectedEdgesTextEdit->setPlainText(getSelectedEdgeListText());
    }

}


void MainWindow::getSelectedNodeInfo(int & selectedNodeCount, QString & selectedNodeCountText, QString & selectedNodeListText, QString & selectedNodeLengthText)
{
    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();

    selectedNodeCount = int(selectedNodes.size());
    selectedNodeCountText = formatIntForDisplay(selectedNodeCount);

    long long totalLength = 0;

    for (int i = 0; i < selectedNodeCount; ++i)
    {
        QString nodeName = selectedNodes[i]->getName();

        //If we are in single mode, don't include +/i in the node name
        if (!g_settings->doubleMode)
            nodeName.chop(1);

        selectedNodeListText += nodeName;
        if (i != int(selectedNodes.size()) - 1)
            selectedNodeListText += ", ";

        totalLength += selectedNodes[i]->getLength();
    }

    selectedNodeLengthText = formatIntForDisplay(totalLength);
}



bool compareEdgePointers(DeBruijnEdge * a, DeBruijnEdge * b)
{
    QString aStart = a->getStartingNode()->getName();
    QString bStart = b->getStartingNode()->getName();
    QString aStartNoSign = aStart;
    aStartNoSign.chop(1);
    QString bStartNoSign = bStart;
    bStartNoSign.chop(1);
    bool ok1;
    long long aStartNumber = aStartNoSign.toLongLong(&ok1);
    bool ok2;
    long long bStartNumber = bStartNoSign.toLongLong(&ok2);

    QString aEnd = a->getEndingNode()->getName();
    QString bEnd = b->getEndingNode()->getName();
    QString aEndNoSign = aEnd;
    aEndNoSign.chop(1);
    QString bEndNoSign = bEnd;
    bEndNoSign.chop(1);
    bool ok3;
    long long aEndNumber = aEndNoSign.toLongLong(&ok3);
    bool ok4;
    long long bEndNumber = bEndNoSign.toLongLong(&ok4);


    //If the node names are essentially numbers, then sort them as numbers.
    if (ok1 && ok2 && ok3 && ok4)
    {
        if (aStartNumber != bStartNumber)
            return aStartNumber < bStartNumber;

        if (aStartNumber == bStartNumber)
            return aEndNumber < bEndNumber;
    }

    //If the node names are strings, then just sort them as strings.
    return aStart < bStart;
}

QString MainWindow::getSelectedEdgeListText()
{
    std::vector<DeBruijnEdge *> selectedEdges = m_scene->getSelectedEdges();

    std::sort(selectedEdges.begin(), selectedEdges.end(), compareEdgePointers);

    QString edgeText;
    for (size_t i = 0; i < selectedEdges.size(); ++i)
    {
        edgeText += selectedEdges[i]->getStartingNode()->getName();
        edgeText += " to ";
        edgeText += selectedEdges[i]->getEndingNode()->getName();
        if (i != selectedEdges.size() - 1)
            edgeText += ", ";
    }

    return edgeText;
}



//This function shows/hides UI elements depending on which
//graph scope is currently selected.  It also reorganises
//the widgets in the layout to prevent gaps when widgets
//are hidden.
void MainWindow::graphScopeChanged()
{
    switch (ui->graphScopeComboBox->currentIndex())
    {
    case 0:
        g_settings->graphScope = WHOLE_GRAPH;

        ui->startingNodesInfoText->setVisible(false);
        ui->startingNodesLabel->setVisible(false);
        ui->startingNodesLineEdit->setVisible(false);

        ui->startingNodesMatchTypeInfoText->setVisible(false);
        ui->startingNodesMatchTypeLabel->setVisible(false);
        ui->startingNodesExactMatchRadioButton->setVisible(false);
        ui->startingNodesPartialMatchRadioButton->setVisible(false);

        ui->nodeDistanceInfoText->setVisible(false);
        ui->nodeDistanceLabel->setVisible(false);
        ui->nodeDistanceSpinBox->setVisible(false);

        ui->graphDrawingGridLayout->addWidget(ui->nodeStyleInfoText, 1, 0, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeStyleLabel, 1, 1, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeStyleWidget, 1, 2, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->drawGraphInfoText, 2, 0, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->drawGraphButton, 2, 1, 1, 2);

        break;

    case 1:
        g_settings->graphScope = AROUND_NODE;

        ui->startingNodesInfoText->setVisible(true);
        ui->startingNodesLabel->setVisible(true);
        ui->startingNodesLineEdit->setVisible(true);

        ui->startingNodesMatchTypeInfoText->setVisible(true);
        ui->startingNodesMatchTypeLabel->setVisible(true);
        ui->startingNodesExactMatchRadioButton->setVisible(true);
        ui->startingNodesPartialMatchRadioButton->setVisible(true);

        ui->nodeDistanceInfoText->setVisible(true);
        ui->nodeDistanceLabel->setVisible(true);
        ui->nodeDistanceSpinBox->setVisible(true);

        ui->nodeDistanceInfoText->setInfoText("Nodes will be drawn if they are specified in the above list or are "
                                              "within this many steps of those nodes.<br><br>"
                                              "A value of 0 will result in only the specified nodes being drawn. "
                                              "A large value will result in large sections of the graph around "
                                              "the specified nodes being drawn.");

        ui->graphDrawingGridLayout->addWidget(ui->startingNodesInfoText, 1, 0, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->startingNodesLabel, 1, 1, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->startingNodesLineEdit, 1, 2, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->startingNodesMatchTypeInfoText, 2, 0, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->startingNodesMatchTypeLabel, 2, 1, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->startingNodesMatchTypeWidget, 2, 2, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeDistanceInfoText, 3, 0, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeDistanceLabel, 3, 1, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeDistanceSpinBox, 3, 2, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeStyleInfoText, 4, 0, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeStyleLabel, 4, 1, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeStyleWidget, 4, 2, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->drawGraphInfoText, 5, 0, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->drawGraphButton, 5, 1, 1, 2);

        break;

    case 2:
        g_settings->graphScope = AROUND_BLAST_HITS;

        ui->startingNodesInfoText->setVisible(false);
        ui->startingNodesLabel->setVisible(false);
        ui->startingNodesLineEdit->setVisible(false);

        ui->startingNodesMatchTypeInfoText->setVisible(false);
        ui->startingNodesMatchTypeLabel->setVisible(false);
        ui->startingNodesExactMatchRadioButton->setVisible(false);
        ui->startingNodesPartialMatchRadioButton->setVisible(false);

        ui->nodeDistanceInfoText->setVisible(true);
        ui->nodeDistanceLabel->setVisible(true);
        ui->nodeDistanceSpinBox->setVisible(true);

        ui->nodeDistanceInfoText->setInfoText("Nodes will be drawn if they contain a BLAST hit or are within this "
                                              "many steps of nodes with a BLAST hit.<br><br>"
                                              "A value of 0 will result in only nodes with BLAST hits being drawn. "
                                              "A large value will result in large sections of the graph around "
                                              "nodes with BLAST hits being drawn.");

        ui->graphDrawingGridLayout->addWidget(ui->nodeDistanceInfoText, 1, 0, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeDistanceLabel, 1, 1, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeDistanceSpinBox, 1, 2, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeStyleInfoText, 2, 0, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeStyleLabel, 2, 1, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->nodeStyleWidget, 2, 2, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->drawGraphInfoText, 3, 0, 1, 1);
        ui->graphDrawingGridLayout->addWidget(ui->drawGraphButton, 3, 1, 1, 2);

        break;
    }
}



void MainWindow::drawGraph()
{
    QString errorTitle;
    QString errorMessage;
    std::vector<DeBruijnNode *> startingNodes = g_assemblyGraph->getStartingNodes(&errorTitle, &errorMessage,
                                                                                  ui->doubleNodesRadioButton->isChecked(),
                                                                                  ui->startingNodesLineEdit->text(),
                                                                                  ui->blastQueryComboBox->currentText());

    if (errorMessage != "")
    {
        QMessageBox::information(this, errorTitle, errorMessage);
        return;
    }

    resetScene();
    g_assemblyGraph->buildOgdfGraphFromNodesAndEdges(startingNodes, g_settings->nodeDistance);
    layoutGraph();
}


void MainWindow::graphLayoutFinished()
{
    delete m_fmmm;
    m_layoutThread = 0;
    g_assemblyGraph->addGraphicsItemsToScene(m_scene);
    m_scene->setSceneRectangle();
    zoomToFitScene();
    selectionChanged();

    setUiState(GRAPH_DRAWN);

    //Move the focus to the view so the user can use keyboard controls to navigate.
    g_graphicsView->setFocus();
}


void MainWindow::graphLayoutCancelled()
{
    m_fmmm->fixedIterations(0);
    m_fmmm->fineTuningIterations(0);
    m_fmmm->threshold(std::numeric_limits<double>::max());
}


void MainWindow::resetScene()
{
    m_scene->blockSignals(true);

    g_assemblyGraph->resetEdges();
    g_assemblyGraph->m_contiguitySearchDone = false;

    g_graphicsView->setScene(0);
    delete m_scene;
    m_scene = new MyGraphicsScene(this);

    g_graphicsView->setScene(m_scene);
    connect(m_scene, SIGNAL(selectionChanged()), this, SLOT(selectionChanged()));
    selectionChanged();

    //Undo the graphics view rotation
    g_graphicsView->rotate(-g_graphicsView->m_rotation);
    g_graphicsView->m_rotation = 0.0;
}


std::vector<DeBruijnNode *> MainWindow::getNodesFromLineEdit(QLineEdit * lineEdit, bool exactMatch, std::vector<QString> * nodesNotInGraph)
{
    return g_assemblyGraph->getNodesFromString(lineEdit->text(), exactMatch, nodesNotInGraph);
}




void MainWindow::layoutGraph()
{
    //The actual layout is done in a different thread so the UI will stay responsive.
    MyProgressDialog * progress = new MyProgressDialog(this, "Laying out graph...", true, "Cancel layout", "Cancelling layout...",
                                                       "Clicking this button will halt the graph layout and display "
                                                       "the graph in its current, incomplete state.<br><br>"
                                                       "Layout can take a long time for very large graphs.  There are "
                                                       "three strategies to reduce the amount of time required:<ul>"
                                                       "<li>Change the scope of the graph from 'Entire graph' to either "
                                                       "'Around nodes' or 'Around BLAST hits'.  This will reduce the "
                                                       "number of nodes that are drawn to the screen.</li>"
                                                       "<li>Increase the 'Base pairs per segment' setting.  This will "
                                                       "result in shorter contigs which take less time to lay out.</li>"
                                                       "<li>Reduce the 'Graph layout iterations' setting.</li></ul>");
    progress->setWindowModality(Qt::WindowModal);
    progress->show();

    m_fmmm = new ogdf::FMMMLayout();

    m_layoutThread = new QThread;
    GraphLayoutWorker * graphLayoutWorker = new GraphLayoutWorker(m_fmmm, g_assemblyGraph->m_graphAttributes,
                                                                  g_settings->graphLayoutQuality, g_settings->segmentLength);
    graphLayoutWorker->moveToThread(m_layoutThread);

    connect(progress, SIGNAL(halt()), this, SLOT(graphLayoutCancelled()));
    connect(m_layoutThread, SIGNAL(started()), graphLayoutWorker, SLOT(layoutGraph()));
    connect(graphLayoutWorker, SIGNAL(finishedLayout()), m_layoutThread, SLOT(quit()));
    connect(graphLayoutWorker, SIGNAL(finishedLayout()), graphLayoutWorker, SLOT(deleteLater()));
    connect(graphLayoutWorker, SIGNAL(finishedLayout()), this, SLOT(graphLayoutFinished()));
    connect(m_layoutThread, SIGNAL(finished()), m_layoutThread, SLOT(deleteLater()));
    connect(m_layoutThread, SIGNAL(finished()), progress, SLOT(deleteLater()));
    m_layoutThread->start();
}




void MainWindow::zoomSpinBoxChanged()
{
    double newValue = ui->zoomSpinBox->value();
    double zoomFactor = newValue / m_previousZoomSpinBoxValue;
    setZoomSpinBoxStep();

    m_graphicsViewZoom->gentle_zoom(zoomFactor, SPIN_BOX);

    m_previousZoomSpinBoxValue = newValue;
}

void MainWindow::setZoomSpinBoxStep()
{
    double newSingleStep = ui->zoomSpinBox->value() * (g_settings->zoomFactor - 1.0) * 100.0;

    //Round up to nearest 0.1
    newSingleStep = int((newSingleStep + 0.1) * 10.0) / 10.0;

    ui->zoomSpinBox->setSingleStep(newSingleStep);

}


void MainWindow::zoomedWithMouseWheel()
{
    ui->zoomSpinBox->blockSignals(true);
    double newSpinBoxValue = g_absoluteZoom * 100.0;
    ui->zoomSpinBox->setValue(newSpinBoxValue);
    setZoomSpinBoxStep();
    m_previousZoomSpinBoxValue = newSpinBoxValue;
    ui->zoomSpinBox->blockSignals(false);
}



void MainWindow::zoomToFitScene()
{
    zoomToFitRect(m_scene->sceneRect());
}


void MainWindow::zoomToFitRect(QRectF rect)
{
    double startingZoom = g_graphicsView->transform().m11();
    g_graphicsView->fitInView(rect, Qt::KeepAspectRatio);
    double endingZoom = g_graphicsView->transform().m11();
    double zoomFactor = endingZoom / startingZoom;
    g_absoluteZoom *= zoomFactor;
    double newSpinBoxValue = ui->zoomSpinBox->value() * zoomFactor;

    //Limit the zoom to the minimum and maximum
    if (g_absoluteZoom < g_settings->minZoom)
    {
        double newZoomFactor = g_settings->minZoom / g_absoluteZoom;
        m_graphicsViewZoom->gentle_zoom(newZoomFactor, SPIN_BOX);
        g_absoluteZoom *= newZoomFactor;
        g_absoluteZoom = g_settings->minZoom;
        newSpinBoxValue = g_settings->minZoom * 100.0;
    }
    if (g_absoluteZoom > g_settings->maxZoom)
    {
        double newZoomFactor = g_settings->maxZoom / g_absoluteZoom;
        m_graphicsViewZoom->gentle_zoom(newZoomFactor, SPIN_BOX);
        g_absoluteZoom *= newZoomFactor;
        g_absoluteZoom = g_settings->maxZoom;
        newSpinBoxValue = g_settings->maxZoom * 100.0;
    }

    ui->zoomSpinBox->blockSignals(true);
    ui->zoomSpinBox->setValue(newSpinBoxValue);
    m_previousZoomSpinBoxValue = newSpinBoxValue;
    ui->zoomSpinBox->blockSignals(false);
}



//This function copies selected sequences to clipboard, if any sequences are
//selected.  If there aren't, then it will prompt the user.
void MainWindow::copySelectedSequencesToClipboardActionTriggered()
{
    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();
    if (selectedNodes.size() == 0)
        QMessageBox::information(this, "Copy sequences to clipboard", "No nodes are selected.\n\n"
                                                                      "You must first select nodes in the graph before you can copy their sequences to the clipboard.");
    else
        copySelectedSequencesToClipboard();
}


//This function copies selected sequences to clipboard, if any sequences are
//selected.
void MainWindow::copySelectedSequencesToClipboard()
{
    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();
    if (selectedNodes.size() == 0)
        return;

    QClipboard * clipboard = QApplication::clipboard();
    QString clipboardText;

    for (size_t i = 0; i < selectedNodes.size(); ++i)
    {
        clipboardText += selectedNodes[i]->getSequence();
        if (i != selectedNodes.size() - 1)
            clipboardText += "\n";
    }

    clipboard->setText(clipboardText);
}


//This function saves selected sequences to file, with a save file prompt, if
//any sequences are selected.  If there aren't, then it will prompt the user.
void MainWindow::saveSelectedSequencesToFileActionTriggered()
{
    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();
    if (selectedNodes.size() == 0)
        QMessageBox::information(this, "Save sequences to FASTA", "No nodes are selected.\n\n"
                                                                  "You must first select nodes in the graph before you can save their sequences to a FASTA file.");
    else
        saveSelectedSequencesToFile();
}


//This function saves selected sequences to file, with a save file prompt, if
//any sequences are selected.
void MainWindow::saveSelectedSequencesToFile()
{
    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();
    if (selectedNodes.size() == 0)
        return;

    QString defaultFileNameAndPath = g_memory->rememberedPath + "/selected_sequences.fasta";

    QString fullFileName = QFileDialog::getSaveFileName(this, "Save node sequences", defaultFileNameAndPath, "FASTA (*.fasta)");

    if (fullFileName != "") //User did not hit cancel
    {
        QFile file(fullFileName);
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&file);

        for (size_t i = 0; i < selectedNodes.size(); ++i)
            out << selectedNodes[i]->getFasta();

        g_memory->rememberedPath = QFileInfo(fullFileName).absolutePath();
    }
}

void MainWindow::copySelectedPathToClipboard()
{
    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();
    if (selectedNodes.size() == 0)
    {
        QMessageBox::information(this, "Copy path sequence to clipboard", "No nodes are selected.\n\n"
                                                                          "You must first select nodes in the graph which define a unambiguous "
                                                                          "path before you can copy their path sequence to the clipboard.");
        return;
    }

    Path nodePath = Path::makeFromUnorderedNodes(selectedNodes, g_settings->doubleMode);
    if (nodePath.isEmpty())
    {
        QMessageBox::information(this, "Copy path sequence to clipboard", "Invalid path.\n\n"
                                                                          "To use copy a path sequence to the clipboard, the nodes must follow "
                                                                          "an unambiguous path through the graph.\n\n"
                                                                          "Complex paths can be defined using the '" + ui->actionSpecify_exact_path_for_copy_save->text() +
                                                                          "' tool.");
        return;
    }

    QClipboard * clipboard = QApplication::clipboard();
    clipboard->setText(nodePath.getPathSequence());
}



void MainWindow::saveSelectedPathToFile()
{
    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();
    if (selectedNodes.size() == 0)
    {
        QMessageBox::information(this, "Save path sequence to FASTA", "No nodes are selected.\n\n"
                                                                      "You must first select nodes in the graph which define a unambiguous "
                                                                      "path before you can save their path sequence to a FASTA file.");
        return;
    }
    Path nodePath = Path::makeFromUnorderedNodes(selectedNodes, g_settings->doubleMode);
    if (nodePath.isEmpty())
    {
        QMessageBox::information(this, "Save path sequence to FASTA", "Invalid path.\n\n"
                                                                      "To use copy a path sequence to the clipboard, the nodes must follow "
                                                                      "an unambiguous path through the graph.\n\n"
                                                                      "Complex paths can be defined using the '" + ui->actionSpecify_exact_path_for_copy_save->text() +
                                                                      "' tool.");
        return;
    }

    QString defaultFileNameAndPath = g_memory->rememberedPath + "/path_sequence.fasta";

    QString fullFileName = QFileDialog::getSaveFileName(this, "Save path sequence", defaultFileNameAndPath, "FASTA (*.fasta)");

    if (fullFileName != "") //User did not hit cancel
    {
        QFile file(fullFileName);
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&file);
        out << nodePath.getFasta();
        g_memory->rememberedPath = QFileInfo(fullFileName).absolutePath();
    }
}




void MainWindow::switchColourScheme()
{
    switch (ui->coloursComboBox->currentIndex())
    {
    case 0:
        g_settings->nodeColourScheme = RANDOM_COLOURS;
        ui->contiguityButton->setVisible(false);
        ui->contiguityInfoText->setVisible(false);
        break;
    case 1:
        g_settings->nodeColourScheme = ONE_COLOUR;
        ui->contiguityButton->setVisible(false);
        ui->contiguityInfoText->setVisible(false);
        break;
    case 2:
        g_settings->nodeColourScheme = READ_DEPTH_COLOUR;
        ui->contiguityButton->setVisible(false);
        ui->contiguityInfoText->setVisible(false);
        break;
    case 3:
        g_settings->nodeColourScheme = BLAST_HITS_SOLID_COLOUR;
        ui->contiguityButton->setVisible(false);
        ui->contiguityInfoText->setVisible(false);
        break;
    case 4:
        g_settings->nodeColourScheme = BLAST_HITS_RAINBOW_COLOUR;
        ui->contiguityButton->setVisible(false);
        ui->contiguityInfoText->setVisible(false);
        break;
    case 5:
        g_settings->nodeColourScheme = CONTIGUITY_COLOUR;
        ui->contiguityButton->setVisible(true);
        ui->contiguityInfoText->setVisible(true);
        break;
    case 6:
        g_settings->nodeColourScheme = CUSTOM_COLOURS;
        ui->contiguityButton->setVisible(false);
        ui->contiguityInfoText->setVisible(false);
        break;
    case 7:
        g_settings->nodeColourScheme = BARCODE_COLOR;
        ui->contiguityButton->setVisible(false);
        ui->contiguityInfoText->setVisible(false);
        break;

    }

    g_assemblyGraph->resetAllNodeColours();
    g_graphicsView->viewport()->update();
}



void MainWindow::determineContiguityFromSelectedNode()
{
    g_assemblyGraph->resetNodeContiguityStatus();
    g_assemblyGraph->resetAllNodeColours();

    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();
    if (selectedNodes.size() > 0)
    {
        MyProgressDialog progress(this, "Determining contiguity...", false);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();

        for (size_t i = 0; i < selectedNodes.size(); ++i)
            (selectedNodes[i])->determineContiguity();

        g_assemblyGraph->m_contiguitySearchDone = true;
        g_assemblyGraph->resetAllNodeColours();
        g_graphicsView->viewport()->update();
    }
    else
        QMessageBox::information(this, "No nodes selected", "Please select one or more nodes for which "
                                                            "contiguity is to be determined.");
}


QString MainWindow::getDefaultImageFileName()
{
    QString fileNameAndPath = g_memory->rememberedPath + "/graph";

    if (m_imageFilter == "PNG (*.png)")
        fileNameAndPath += ".png";
    else if (m_imageFilter == "JPEG (*.jpg)")
        fileNameAndPath += ".jpg";
    else if (m_imageFilter == "SVG (*.svg)")
        fileNameAndPath += ".svg";
    else
        fileNameAndPath += ".png";

    return fileNameAndPath;
}


void MainWindow::saveImageCurrentView()
{
    if (!checkForImageSave())
        return;

    QString defaultFileNameAndPath = getDefaultImageFileName();

    QString selectedFilter = m_imageFilter;
    QString fullFileName = QFileDialog::getSaveFileName(this, "Save graph image (current view)",
                                                        defaultFileNameAndPath,
                                                        "PNG (*.png);;JPEG (*.jpg);;SVG (*.svg)",
                                                        &selectedFilter);

    bool pixelImage = true;
    if (selectedFilter == "PNG (*.png)" || selectedFilter == "JPEG (*.jpg)")
        pixelImage = true;
    else if (selectedFilter == "SVG (*.svg)")
        pixelImage = false;

    if (fullFileName != "") //User did not hit cancel
    {
        m_imageFilter = selectedFilter;

        QPainter painter;
        if (pixelImage)
        {
            QImage image(g_graphicsView->viewport()->rect().size(), QImage::Format_ARGB32);
            image.fill(Qt::white);
            painter.begin(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            g_graphicsView->render(&painter);
            image.save(fullFileName);
            g_memory->rememberedPath = QFileInfo(fullFileName).absolutePath();
            painter.end();
        }
        else //SVG
        {
            QSvgGenerator generator;
            generator.setFileName(fullFileName);
            QSize size = g_graphicsView->viewport()->rect().size();
            generator.setSize(size);
            generator.setViewBox(QRect(0, 0, size.width(), size.height()));
            painter.begin(&generator);
            painter.fillRect(0, 0, size.width(), size.height(), Qt::white);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            g_graphicsView->render(&painter);
            painter.end();
        }
    }
}

void MainWindow::saveImageEntireScene()
{
    if (!checkForImageSave())
        return;

    QString defaultFileNameAndPath = getDefaultImageFileName();

    QString selectedFilter = m_imageFilter;
    QString fullFileName = QFileDialog::getSaveFileName(this,
                                                        "Save graph image (entire scene)",
                                                        defaultFileNameAndPath,
                                                        "PNG (*.png);;JPEG (*.jpg);;SVG (*.svg)",
                                                        &selectedFilter);

    bool pixelImage = true;
    if (selectedFilter == "PNG (*.png)" || selectedFilter == "JPEG (*.jpg)")
        pixelImage = true;
    else if (selectedFilter == "SVG (*.svg)")
        pixelImage = false;

    if (fullFileName != "") //User did not hit cancel
    {
        //The positionTextNodeCentre setting must be used for the entire scene
        //or else only the labels in the current viewport will be shown.
        bool positionTextNodeCentreSettingBefore = g_settings->positionTextNodeCentre;
        g_settings->positionTextNodeCentre = true;

        //Temporarily undo any rotation so labels appear upright.
        double rotationBefore = g_graphicsView->m_rotation;
        g_graphicsView->m_rotation = 0.0;

        m_imageFilter = selectedFilter;

        QPainter painter;
        if (pixelImage)
        {
            QSize imageSize = g_absoluteZoom * m_scene->sceneRect().size().toSize();

            if (imageSize.width() > 32767 || imageSize.height() > 32767)
            {
                QString error = "Images can not be taller or wider than 32767 pixels, but at the "
                                "current zoom level, the image to be saved would be ";
                error += QString::number(imageSize.width()) + "x" + QString::number(imageSize.height()) + " pixels.\n\n";
                error += "Please reduce the zoom level before saving the entire scene to image or use the SVG format.";

                QMessageBox::information(this, "Image too large", error);
                return;
            }

            if (imageSize.width() * imageSize.height() > 50000000) //50 megapixels is used as an arbitrary large image cutoff
            {
                QString warning = "At the current zoom level, the image will be ";
                warning += QString::number(imageSize.width()) + "x" + QString::number(imageSize.height()) + " pixels. ";
                warning += "An image of this large size may take significant time and space to save.\n\n"
                           "The image size can be reduced by decreasing the zoom level or using the SVG format.\n\n"
                           "Do you want to continue saving the image?";
                QMessageBox::StandardButton response = QMessageBox::question(this, "Large image", warning);
                if (response == QMessageBox::No || response == QMessageBox::Cancel)
                    return;
            }

            QImage image(imageSize, QImage::Format_ARGB32);
            image.fill(Qt::white);
            painter.begin(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            m_scene->setSceneRectangle();
            m_scene->render(&painter);
            image.save(fullFileName);
            g_memory->rememberedPath = QFileInfo(fullFileName).absolutePath();
            painter.end();
        }
        else //SVG
        {
            QSvgGenerator generator;
            generator.setFileName(fullFileName);
            QSize size = g_absoluteZoom * m_scene->sceneRect().size().toSize();
            generator.setSize(size);
            generator.setViewBox(QRect(0, 0, size.width(), size.height()));
            painter.begin(&generator);
            painter.fillRect(0, 0, size.width(), size.height(), Qt::white);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::TextAntialiasing);
            m_scene->setSceneRectangle();
            m_scene->render(&painter);
            painter.end();
        }

        g_settings->positionTextNodeCentre = positionTextNodeCentreSettingBefore;
        g_graphicsView->m_rotation = rotationBefore;
    }
}



//This function makes sure that a graph is loaded and drawn so that an image can be saved.
//It returns true if everything is fine.  If things aren't ready, it displays a message
//to the user and returns false.
bool MainWindow::checkForImageSave()
{
    if (m_uiState == NO_GRAPH_LOADED)
    {
        QMessageBox::information(this, "No image to save", "You must first load and then draw a graph before you can save an image to file.");
        return false;
    }
    if (m_uiState == GRAPH_LOADED)
    {
        QMessageBox::information(this, "No image to save", "You must first draw the graph before you can save an image to file.");
        return false;
    }
    return true;
}


void MainWindow::setTextDisplaySettings()
{
    g_settings->displayNodeCustomLabels = ui->nodeCustomLabelsCheckBox->isChecked();
    g_settings->displayNodeNames = ui->nodeNamesCheckBox->isChecked();
    g_settings->displayNodeLengths = ui->nodeLengthsCheckBox->isChecked();
    g_settings->displayNodeReadDepth = ui->nodeReadDepthCheckBox->isChecked();
    g_settings->displayBlastHits = ui->blastHitsCheckBox->isChecked();
    g_settings->textOutline = ui->textOutlineCheckBox->isChecked();

    g_graphicsView->viewport()->update();
}


void MainWindow::fontButtonPressed()
{
    bool ok;
    g_settings->labelFont = QFontDialog::getFont(&ok, g_settings->labelFont, this);
    if (ok)
        g_graphicsView->viewport()->update();
}



void MainWindow::setNodeCustomColour()
{
    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();
    if (selectedNodes.size() == 0)
        return;

    QString dialogTitle = "Select custom colour for selected node";
    if (selectedNodes.size() > 1)
        dialogTitle += "s";

    QColor newColour = QColorDialog::getColor(selectedNodes[0]->getCustomColour(), this, dialogTitle);
    if (newColour.isValid())
    {
        //If the colouring scheme is not currently custom, change it to custom now
        if (g_settings->nodeColourScheme != CUSTOM_COLOURS)
            setNodeColourSchemeComboBox(CUSTOM_COLOURS);

        for (size_t i = 0; i < selectedNodes.size(); ++i)
        {
            selectedNodes[i]->setCustomColour(newColour);
            if (selectedNodes[i]->getGraphicsItemNode() != 0)
                selectedNodes[i]->getGraphicsItemNode()->setNodeColour();

        }
        g_graphicsView->viewport()->update();
    }
}

void MainWindow::setNodeCustomLabel()
{
    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();
    if (selectedNodes.size() == 0)
        return;

    QString dialogMessage = "Type a custom label for selected node";
    if (selectedNodes.size() > 1)
        dialogMessage += "s";
    dialogMessage += ":";

    bool ok;
    QString newLabel = QInputDialog::getText(this, "Custom label", dialogMessage, QLineEdit::Normal,
                                             selectedNodes[0]->getCustomLabel(), &ok);

    if (ok)
    {
        //If the custom label option isn't currently on, turn it on now.
        ui->nodeCustomLabelsCheckBox->setChecked(true);

        for (size_t i = 0; i < selectedNodes.size(); ++i)
            selectedNodes[i]->setCustomLabel(newLabel);
    }
}


void MainWindow::removeNodes()
{
    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();
    if (selectedNodes.size() == 0)
        return;

    for (size_t i = 0; i < selectedNodes.size(); ++i)
    {
        //First remove any edges connected to this node
        removeAllGraphicsEdgesFromNode(selectedNodes[i]);

        //If the graph is on single mode, then also try to remove any
        //edges connected to the reverse complement node
        if (!g_settings->doubleMode)
            removeAllGraphicsEdgesFromNode(selectedNodes[i]->getReverseComplement());

        //Now remove the node itself
        GraphicsItemNode * graphicsItemNode = selectedNodes[i]->getGraphicsItemNode();
        m_scene->removeItem(graphicsItemNode);
        delete graphicsItemNode;
        selectedNodes[i]->setGraphicsItemNode(0);
    }
}

void MainWindow::removeAllGraphicsEdgesFromNode(DeBruijnNode * node)
{
    const std::vector<DeBruijnEdge *> * edges = node->getEdgesPointer();
    for (size_t i = 0; i < edges->size(); ++i)
    {
        DeBruijnEdge * deBruijnEdge = (*edges)[i];
        GraphicsItemEdge * graphicsItemEdge = deBruijnEdge->getGraphicsItemEdge();
        if (graphicsItemEdge != 0)
        {
            m_scene->removeItem(graphicsItemEdge);
            delete graphicsItemEdge;
            deBruijnEdge->setGraphicsItemEdge(0);
        }
    }
}



void MainWindow::openSettingsDialog()
{
    SettingsDialog settingsDialog(this);
    settingsDialog.setWidgetsFromSettings();

    if (settingsDialog.exec()) //The user clicked OK
    {
        Settings settingsBefore = *g_settings;

        settingsDialog.setSettingsFromWidgets();

        //If the settings affecting node width was changed, reset the width on
        //each GraphicsItemNode.
        if (settingsBefore.readDepthEffectOnWidth != g_settings->readDepthEffectOnWidth ||
                settingsBefore.readDepthPower != g_settings->readDepthPower)
            g_assemblyGraph->recalculateAllNodeWidths();

        //If any of the colours changed, reset the node colours now.
        if (settingsBefore.uniformPositiveNodeColour != g_settings->uniformPositiveNodeColour ||
                settingsBefore.uniformNegativeNodeColour != g_settings->uniformNegativeNodeColour ||
                settingsBefore.uniformNodeSpecialColour != g_settings->uniformNodeSpecialColour ||
                settingsBefore.autoReadDepthValue != g_settings->autoReadDepthValue ||
                settingsBefore.lowReadDepthColour != g_settings->lowReadDepthColour ||
                settingsBefore.highReadDepthColour != g_settings->highReadDepthColour ||
                settingsBefore.lowReadDepthValue != g_settings->lowReadDepthValue ||
                settingsBefore.highReadDepthValue != g_settings->highReadDepthValue ||
                settingsBefore.noBlastHitsColour != g_settings->noBlastHitsColour ||
                settingsBefore.contiguousStrandSpecificColour != g_settings->contiguousStrandSpecificColour ||
                settingsBefore.contiguousEitherStrandColour != g_settings->contiguousEitherStrandColour ||
                settingsBefore.notContiguousColour != g_settings->notContiguousColour ||
                settingsBefore.maybeContiguousColour != g_settings->maybeContiguousColour ||
                settingsBefore.contiguityStartingColour != g_settings->contiguityStartingColour ||
                settingsBefore.randomColourPositiveOpacity != g_settings->randomColourPositiveOpacity ||
                settingsBefore.randomColourNegativeOpacity != g_settings->randomColourNegativeOpacity ||
                settingsBefore.randomColourPositiveSaturation != g_settings->randomColourPositiveSaturation ||
                settingsBefore.randomColourNegativeSaturation != g_settings->randomColourNegativeSaturation ||
                settingsBefore.randomColourPositiveLightness != g_settings->randomColourPositiveLightness ||
                settingsBefore.randomColourNegativeLightness != g_settings->randomColourNegativeLightness)
        {
            g_assemblyGraph->resetAllNodeColours();
        }

        g_graphicsView->setAntialiasing(g_settings->antialiasing);
        g_graphicsView->viewport()->update();
    }
}



void MainWindow::selectUserSpecifiedBarcodes()
{

    if (g_barcode_manager->barcode_selected.contains(ui->barcodeInput->text().simplified()))
    {
        QMessageBox::warning(NULL, "Warning", "Barcode already set!");
        ui->barcodeInput->clear();
        return;
    }

    if (!g_barcode_manager->barocde_map.contains(ui->barcodeInput->text().simplified())){
        QMessageBox::warning(NULL, "Warning", "Barcode don\'t exist in data!");
        ui->barcodeInput->clear();
        return;
    }

    //g_barcode_manager->barcode_selected.append(ui->barcodeInput->text().simplified());
    g_barcode_manager->add_barcode(ui->barcodeInput->text().simplified());

    //g_barcode_manager->add_barcode(ui->barcodeInput->text());
    ui->barcodeInput->clear();
    //for (int i = 0; i<g_barcode_manager->barcode_selected.size(); i++ )
    //    qDebug() << g_barcode_manager->barcode_selected.at(i);
    //qDebug() << ui->barcodeInput->text().simplified();
    fillBarcodeTable();
    refreshDisplay();

    //QMessageBox::information(this, "Barcodes not found", "ohh");
    /*if (g_assemblyGraph->checkIfStringHasNodes(ui->selectionSearchNodesLineEdit->text()))
    {
        QMessageBox::information(this, "No starting nodes",
                                 "Please enter at least one node when drawing the graph using the 'Around node(s)' scope. "
                                 "Separate multiple nodes with commas.");
        return;
    }




    if (ui->selectionSearchNodesLineEdit->text().length() == 0)
    {
        QMessageBox::information(this, "No nodes given", "Please enter the numbers of the nodes to find, separated by commas.");
        return;
    }

    m_scene->blockSignals(true);
    m_scene->clearSelection();
    std::vector<QString> nodesNotInGraph;
    std::vector<DeBruijnNode *> nodesToSelect = getNodesFromLineEdit(ui->selectionSearchNodesLineEdit,
                                                                     ui->selectionSearchNodesExactMatchRadioButton->isChecked(),
                                                                     &nodesNotInGraph);

    //Select each node that actually has a GraphicsItemNode, and build a bounding
    //rectangle so the viewport can focus on the selected node.
    std::vector<QString> nodesNotFound;
    int foundNodes = 0;
    for (size_t i = 0; i < nodesToSelect.size(); ++i)
    {
        GraphicsItemNode * graphicsItemNode = nodesToSelect[i]->getGraphicsItemNode();

        //If the GraphicsItemNode isn't found, try the reverse complement.  This
        //is only done for single node mode.
        if (graphicsItemNode == 0 && !g_settings->doubleMode)
            graphicsItemNode = nodesToSelect[i]->getReverseComplement()->getGraphicsItemNode();

        if (graphicsItemNode != 0)
        {
            graphicsItemNode->setSelected(true);
            ++foundNodes;
        }
        else
            nodesNotFound.push_back(nodesToSelect[i]->getName());
    }

    if (foundNodes > 0)
        zoomToSelection();

    if (nodesNotInGraph.size() > 0 || nodesNotFound.size() > 0)
    {
        QString errorMessage;
        if (nodesNotInGraph.size() > 0)
        {
            errorMessage += g_assemblyGraph->generateNodesNotFoundErrorMessage(nodesNotInGraph,
                                                                               ui->selectionSearchNodesExactMatchRadioButton->isChecked());
        }
        if (nodesNotFound.size() > 0)
        {
            if (errorMessage.length() > 0)
                errorMessage += "\n";
            errorMessage += "The following nodes are in the graph but not currently displayed:\n";
            for (size_t i = 0; i < nodesNotFound.size(); ++i)
            {
                errorMessage += nodesNotFound[i];
                if (i != nodesNotFound.size() - 1)
                    errorMessage += ", ";
            }
            errorMessage += "\n\nRedraw the graph with an increased scope to see these nodes.\n";
        }
        QMessageBox::information(this, "Nodes not found", errorMessage);
    }

    m_scene->blockSignals(false);
    g_graphicsView->viewport()->update();
    selectionChanged();*/
}






void MainWindow::selectUserSpecifiedNodes()
{
    if (g_assemblyGraph->checkIfStringHasNodes(ui->selectionSearchNodesLineEdit->text()))
    {
        QMessageBox::information(this, "No starting nodes",
                                 "Please enter at least one node when drawing the graph using the 'Around node(s)' scope. "
                                 "Separate multiple nodes with commas.");
        return;
    }




    if (ui->selectionSearchNodesLineEdit->text().length() == 0)
    {
        QMessageBox::information(this, "No nodes given", "Please enter the numbers of the nodes to find, separated by commas.");
        return;
    }

    m_scene->blockSignals(true);
    m_scene->clearSelection();
    std::vector<QString> nodesNotInGraph;
    std::vector<DeBruijnNode *> nodesToSelect = getNodesFromLineEdit(ui->selectionSearchNodesLineEdit,
                                                                     ui->selectionSearchNodesExactMatchRadioButton->isChecked(),
                                                                     &nodesNotInGraph);

    //Select each node that actually has a GraphicsItemNode, and build a bounding
    //rectangle so the viewport can focus on the selected node.
    std::vector<QString> nodesNotFound;
    int foundNodes = 0;
    for (size_t i = 0; i < nodesToSelect.size(); ++i)
    {
        GraphicsItemNode * graphicsItemNode = nodesToSelect[i]->getGraphicsItemNode();

        //If the GraphicsItemNode isn't found, try the reverse complement.  This
        //is only done for single node mode.
        if (graphicsItemNode == 0 && !g_settings->doubleMode)
            graphicsItemNode = nodesToSelect[i]->getReverseComplement()->getGraphicsItemNode();

        if (graphicsItemNode != 0)
        {
            graphicsItemNode->setSelected(true);
            ++foundNodes;
        }
        else
            nodesNotFound.push_back(nodesToSelect[i]->getName());
    }

    if (foundNodes > 0)
        zoomToSelection();

    if (nodesNotInGraph.size() > 0 || nodesNotFound.size() > 0)
    {
        QString errorMessage;
        if (nodesNotInGraph.size() > 0)
        {
            errorMessage += g_assemblyGraph->generateNodesNotFoundErrorMessage(nodesNotInGraph,
                                                                               ui->selectionSearchNodesExactMatchRadioButton->isChecked());
        }
        if (nodesNotFound.size() > 0)
        {
            if (errorMessage.length() > 0)
                errorMessage += "\n";
            errorMessage += "The following nodes are in the graph but not currently displayed:\n";
            for (size_t i = 0; i < nodesNotFound.size(); ++i)
            {
                errorMessage += nodesNotFound[i];
                if (i != nodesNotFound.size() - 1)
                    errorMessage += ", ";
            }
            errorMessage += "\n\nRedraw the graph with an increased scope to see these nodes.\n";
        }
        QMessageBox::information(this, "Nodes not found", errorMessage);
    }

    m_scene->blockSignals(false);
    g_graphicsView->viewport()->update();
    selectionChanged();
}


void MainWindow::openAboutDialog()
{
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}


void MainWindow::openBlastSearchDialog()
{
    //If a BLAST search dialog does not current exist, make it.
    if (m_blastSearchDialog == 0)
    {
        m_blastSearchDialog = new BlastSearchDialog(this);
        connect(m_blastSearchDialog, SIGNAL(blastChanged()), this, SLOT(blastChanged()));
        connect(m_blastSearchDialog, SIGNAL(queryPathSelectionChanged()), g_graphicsView->viewport(), SLOT(update()));
    }

    m_blastSearchDialog->show();
}


//This function is called whenever the user does something in the
//BlastSearchDialog that should be reflected here in MainWindow.
void MainWindow::blastChanged()
{
    QString blastQueryText = ui->blastQueryComboBox->currentText();
    BlastQuery * queryBefore = g_blastSearch->m_blastQueries.getQueryFromName(blastQueryText);

    //If we didn't find a currently selected query but it isn't "none" or "all",
    //then maybe the user changed the name of the currently selected query, and
    //that's why we didn't find it.  In that case, try to find it using the
    //index.
    if (queryBefore == 0 && blastQueryText != "none" && blastQueryText != "all")
    {
        int blastQueryIndex = ui->blastQueryComboBox->currentIndex();
        if (ui->blastQueryComboBox->count() > 1)
            --blastQueryIndex;
        if (blastQueryIndex < g_blastSearch->m_blastQueries.getQueryCount())
            queryBefore = g_blastSearch->m_blastQueries.m_queries[blastQueryIndex];
    }

    //Rebuild the query combo box, in case the user changed the queries or
    //their names.
    setupBlastQueryComboBox();

    //Look to see if the query selected before is still present.  If so,
    //set the combo box to have that query selected.  If not (or if no
    //query was previously selected), leave the combo box a index 0.
    if (queryBefore != 0 && g_blastSearch->m_blastQueries.isQueryPresent(queryBefore))
    {
        int indexOfQuery = ui->blastQueryComboBox->findText(queryBefore->getName());
        if (indexOfQuery != -1)
            ui->blastQueryComboBox->setCurrentIndex(indexOfQuery);
    }

    if (g_blastSearch->m_blastQueries.getQueryCount() > 0)
    {
        //If the colouring scheme is not currently BLAST hits, change it to BLAST hits now
        if (g_settings->nodeColourScheme != BLAST_HITS_RAINBOW_COLOUR &&
                g_settings->nodeColourScheme != BLAST_HITS_SOLID_COLOUR)
        {
            //If there is only one query, use BLAST rainbow.  Otherwise, use
            //BLAST solid.
            if (g_blastSearch->m_blastQueries.getQueryCount() == 1)
                setNodeColourSchemeComboBox(BLAST_HITS_RAINBOW_COLOUR);
            else
                setNodeColourSchemeComboBox(BLAST_HITS_SOLID_COLOUR);
        }
    }

    g_blastSearch->blastQueryChanged(ui->blastQueryComboBox->currentText());
    g_graphicsView->viewport()->update();
}


void MainWindow::setupBlastQueryComboBox()
{
    ui->blastQueryComboBox->clear();
    QStringList comboBoxItems;
    for (size_t i = 0; i < g_blastSearch->m_blastQueries.m_queries.size(); ++i)
    {
        if (g_blastSearch->m_blastQueries.m_queries[i]->hasHits())
            comboBoxItems.push_back(g_blastSearch->m_blastQueries.m_queries[i]->getName());
    }

    if (comboBoxItems.size() > 1)
        comboBoxItems.push_front("all");

    if (comboBoxItems.size() > 0)
    {
        ui->blastQueryComboBox->addItems(comboBoxItems);
        ui->blastQueryComboBox->setEnabled(true);
    }
    else
    {
        ui->blastQueryComboBox->addItem("none");
        ui->blastQueryComboBox->setEnabled(false);
    }
}


void MainWindow::blastQueryChanged()
{
    g_blastSearch->blastQueryChanged(ui->blastQueryComboBox->currentText());
    g_graphicsView->viewport()->update();
}


void MainWindow::setInfoTexts()
{
    QString control = "Ctrl";
    QString settingsDialogTitle = "settings";
#ifdef Q_OS_MAC
    QString command(QChar(0x2318));
    control = command;
    settingsDialogTitle = "preferences";
#endif

    ui->graphInformationInfoText->setInfoText("Node codes, edge count and total length are calculated using single "
                                              "nodes, not double nodes.<br><br>"
                                              "For example, node 5+ and node 5- would only be counted once.");
    ui->graphScopeInfoText->setInfoText("This controls how much of the assembly graph will be drawn:<ul>"
                                        "<li>'Entire graph': all nodes in the graph will be drawn. This is "
                                        "appropriate for smaller graphs, but large graphs may take "
                                        "longer and use large amounts of memory to draw in their entirety.</li>"
                                        "<li>'Around nodes': you can specify nodes and a distance to "
                                        "limit the drawing to a smaller region of the graph.</li>"
                                        "<li>'Around BLAST hits': if you have conducted a BLAST search "
                                        "on this graph, this option will draw the region(s) of the graph "
                                        "around nodes that contain hits.</li></ul>");
    ui->startingNodesInfoText->setInfoText("Enter a comma-delimited list of node names here. This will "
                                           "define which regions of the graph will be drawn.<br><br>"
                                           "When in double mode, you can include '+' or '-' at the end "
                                           "of the node name to specify which strand to draw. If you do "
                                           "not include '+' or '-', then nodes for both strands will be drawn.");
    ui->startingNodesMatchTypeInfoText->setInfoText("When 'Exact' match is used, the graph will only be drawn around nodes "
                                                    "that exactly match your above input.<br><br>"
                                                    "When 'Partial' match is used, the graph will be drawn around "
                                                    "nodes where any part of their name matches your above input.");
    ui->selectionSearchNodesMatchTypeInfoText->setInfoText("When 'Exact' match is used, nodes will only be selected if "
                                                           "their name exactly matches your input above.<br><br>"
                                                           "When 'Partial' match is used, nodes will be selected if any "
                                                           "part of their name matches your input above.");
    ui->nodeStyleInfoText->setInfoText("'Single' mode will only one node for each positive/negative pair. "
                                       "This produces a simpler graph visualisation, but "
                                       "strand-specific sequences and directionality will be less clear.<br><br>"
                                       "'Double' mode will draw both nodes and their complement nodes. The nodes "
                                       "will show directionality with an arrow head. They will initially be "
                                       "drawn on top of each other, but can be manually moved to separate them.");
    ui->drawGraphInfoText->setInfoText("Clicking this button will conduct the graph layout and draw the graph to "
                                       "the screen. This process is fast for small graphs but can be "
                                       "resource-intensive for large graphs.<br><br>"
                                       "The layout algorithm uses a random seed, so each time this button is "
                                       "clicked you will give different layouts of the same graph.");
    ui->zoomInfoText->setInfoText("This value controls how large the graph appears in Bandage. The zoom level "
                                  "can also be changed by:<ul>"
                                  "<li>Holding the " + control + " key and using the mouse wheel over the graph.</li>"
                                  "<li>Clicking on the graph display and then using the '+' and '-' keys.</li></ul>");
    ui->nodeWidthInfoText->setInfoText("This is the average width for each node. The exact width for each node is "
                                       "also influenced by the node's read depth. The effect of read depth on width "
                                       "can be adjusted in Bandage " + settingsDialogTitle + ".");
    ui->nodeColourInfoText->setInfoText("This controls the colour of the nodes in the graph:<ul>"
                                        "<li>'Random colours': Nodes will be coloured randomly. Each time this is "
                                        "selected, new random colours will be chosen. Negative nodes (visible "
                                        "in 'Double' mode) will be a darker shade of their complement positive "
                                        "nodes.</li>"
                                        "<li>'Uniform colour': For graphs drawn with the 'Entire graph' scope, all "
                                        "nodes will be the same colour. For graphs drawn with the 'Around nodes' "
                                        "scope, your specified nodes will be drawn in a separate colour. For "
                                        "graphs drawn with the 'Around BLAST hits' scope, nodes with BLAST hits "
                                        "will be drawn in a separate colour.</li>"
                                        "<li>'Colour by read depth': Node colours will be defined by their "
                                        "read depth. The details of this relationship are configurable in "
                                        "Bandage " + settingsDialogTitle + ".</li>"
                                        "<li>'BLAST hits (rainbow)': Nodes will be drawn in a light grey colour "
                                        "and BLAST hits for the currently selected query will be drawn using a "
                                        "rainbow. Red indicates the start of the query sequence and violet "
                                        "indicates the end.</li>"
                                        "<li>'BLAST hits (solid)': Nodes will be drawn in a light grey colour "
                                        "and BLAST hits for the currently selected query will be drawn using "
                                        "the query's colour. Query colours can be specified in the 'Create/view"
                                        "BLAST search' window.</li>"
                                        "<li>'Colour by contiguity': This option will display a 'Determine "
                                        "contiguity button. When pressed, the nodes will be coloured based "
                                        "on their contiguity with the selected node(s).</li>"
                                        "<li>'Custom colours': Nodes will be coloured using colours of your "
                                        "choice. Select one or more nodes and then click the 'Set colour' button "
                                        "to define their colour.</li></ul>"
                                        "See the 'Colours' section of the Bandage " + settingsDialogTitle + " "
                                        "to control various colouring options.");
    ui->contiguityInfoText->setInfoText("Select one or more nodes and then click this button. Bandage will "
                                        "then colour which other nodes in the graph are likely to be contiguous "
                                        "with your selected node(s).");
    ui->nodeLabelsInfoText->setInfoText("Tick any of the node labelling options to display those labels over "
                                        "nodes in the graph.<br><br>"
                                        "'Name', 'Length' and 'Read depth' labels are created automatically. "
                                        "'Custom' labels must be assigned by clicking the 'Set "
                                        "label' button when one or more nodes are selected.<br><br>"
                                        "When 'BLAST hits' labels are shown, they are displayed over any "
                                        "BLAST hits present in the node.");
    ui->nodeFontInfoText->setInfoText("Click the 'Font' button to choose the font used for node labels. The "
                                      "colour of the font is configurable in Bandage's " + settingsDialogTitle + ".<br><br>"
                                      "Ticking 'Text outline' will surround the text with a white outline. "
                                      "This can help to make text more readable, but will obscure more of the "
                                      "underlying graph. The thickness of the text outline is configurable in "
                                      "Bandage's " + settingsDialogTitle + ".");
    ui->blastSearchInfoText->setInfoText("Click this button to open a dialog where a BLAST search for one "
                                         "or more queries can be carried out on the graph's nodes.<br><br>"
                                         "After a BLAST search is complete, it will be possible to use the "
                                         "'Around BLAST hits' graph scope and the 'BLAST "
                                         "hits' colour modes.");
    ui->blastQueryInfoText->setInfoText("After a BLAST search is completed, you can select a query here for use "
                                        "with the 'Around BLAST hits' graph scope and the 'BLAST "
                                        "hits' colour modes.");
    ui->selectionSearchInfoText->setInfoText("Type a comma-delimited list of one or mode node numbers and then click "
                                             "the 'Find node(s)' button to search for nodes in the graph. "
                                             "If the search is successful, the view will zoom to the found nodes "
                                             "and they will be selected.");
    ui->setColourAndLabelInfoText->setInfoText("Custom colours and labels can be applied to selected nodes using "
                                               "these buttons. They will only be visible when the colouring "
                                               "mode is set to 'Custom colours' and the 'Custom' label option "
                                               "is ticked.");
    ui->removeNodesInfoText->setInfoText("Click this button to remove selected nodes from the drawn graph, along "
                                         "with any edges that connect to those nodes. This makes no change to "
                                         "the underlying assembly graph, just the visualisation.<br><br>"
                                         "To see removed nodes again, you must redraw the graph by clicking "
                                         "'Draw graph'.");
}



void MainWindow::setUiState(UiState uiState)
{
    m_uiState = uiState;

    switch (uiState)
    {
    case NO_GRAPH_LOADED:
        ui->graphDetailsWidget->setEnabled(false);
        ui->graphDrawingWidget->setEnabled(false);
        ui->graphDisplayWidget->setEnabled(false);
        ui->nodeLabelsWidget->setEnabled(false);
        ui->blastSearchWidget->setEnabled(false);
        ui->selectionScrollAreaWidgetContents->setEnabled(false);
        break;
    case GRAPH_LOADED:
        ui->graphDetailsWidget->setEnabled(true);
        ui->graphDrawingWidget->setEnabled(true);
        ui->graphDisplayWidget->setEnabled(false);
        ui->nodeLabelsWidget->setEnabled(false);
        ui->blastSearchWidget->setEnabled(true);
        ui->selectionScrollAreaWidgetContents->setEnabled(false);
        break;
    case GRAPH_DRAWN:
        ui->graphDetailsWidget->setEnabled(true);
        ui->graphDrawingWidget->setEnabled(true);
        ui->graphDisplayWidget->setEnabled(true);
        ui->nodeLabelsWidget->setEnabled(true);
        ui->blastSearchWidget->setEnabled(true);
        ui->selectionScrollAreaWidgetContents->setEnabled(true);
        break;
    }
}


void MainWindow::showHidePanels()
{
    ui->controlsScrollArea->setVisible(ui->actionControls_panel->isChecked());
    ui->selectionScrollArea->setVisible(ui->actionSelection_panel->isChecked());
}


void MainWindow::bringSelectedNodesToFront()
{
    m_scene->blockSignals(true);

    std::vector<DeBruijnNode *> selectedNodes = m_scene->getSelectedNodes();
    if (selectedNodes.size() == 0)
    {
        QMessageBox::information(this, "No nodes selected", "You must first select nodes in the graph before using "
                                                            "the 'Bring selected nodes to front' function.");
        return;
    }

    double topZ = m_scene->getTopZValue();
    double newZ = topZ + 1.0;

    for (size_t i = 0; i < selectedNodes.size(); ++i)
    {
        GraphicsItemNode * graphicsItemNode = selectedNodes[i]->getGraphicsItemNode();

        if (graphicsItemNode == 0)
            continue;

        graphicsItemNode->setZValue(newZ);
    }
    m_scene->blockSignals(false);
    g_graphicsView->viewport()->update();
}


void MainWindow::selectNodesWithBlastHits()
{
    m_scene->blockSignals(true);
    m_scene->clearSelection();

    bool atLeastOneNodeSelected = false;
    QMapIterator<QString, DeBruijnNode*> i(g_assemblyGraph->m_deBruijnGraphNodes);
    while (i.hasNext())
    {
        i.next();
        DeBruijnNode * node = i.value();
        GraphicsItemNode * graphicsItemNode = node->getGraphicsItemNode();

        if (graphicsItemNode == 0)
            continue;

        //If we're in double mode, only select a node if it has a BLAST hit itself.
        if (g_settings->doubleMode)
        {
            if (node->thisNodeHasBlastHits())
            {
                graphicsItemNode->setSelected(true);
                atLeastOneNodeSelected = true;
            }
        }

        //In single mode, select a node if it or its reverse complement has a BLAST hit.
        else
        {
            if (node->thisNodeOrReverseComplementHasBlastHits())
            {
                graphicsItemNode->setSelected(true);
                atLeastOneNodeSelected = true;
            }
        }
    }
    m_scene->blockSignals(false);
    g_graphicsView->viewport()->update();
    selectionChanged();

    if (!atLeastOneNodeSelected)
        QMessageBox::information(this, "No BLAST hits",
                                       "To select nodes with BLAST hits, you must first conduct a BLAST search.");
    else
        zoomToSelection();
}


void MainWindow::selectAll()
{
    m_scene->blockSignals(true);
    QList<QGraphicsItem *> allItems = m_scene->items();
    for (int i = 0; i < allItems.size(); ++i)
    {
        QGraphicsItem * item = allItems[i];
        item->setSelected(true);
    }
    m_scene->blockSignals(false);
    g_graphicsView->viewport()->update();
    selectionChanged();
}


void MainWindow::selectNone()
{
    m_scene->blockSignals(true);
    QList<QGraphicsItem *> allItems = m_scene->items();
    for (int i = 0; i < allItems.size(); ++i)
    {
        QGraphicsItem * item = allItems[i];
        item->setSelected(false);
    }
    m_scene->blockSignals(false);
    g_graphicsView->viewport()->update();
    selectionChanged();
}

void MainWindow::invertSelection()
{
    m_scene->blockSignals(true);
    QList<QGraphicsItem *> allItems = m_scene->items();
    for (int i = 0; i < allItems.size(); ++i)
    {
        QGraphicsItem * item = allItems[i];
        item->setSelected(!item->isSelected());
    }
    m_scene->blockSignals(false);
    g_graphicsView->viewport()->update();
    selectionChanged();
}



void MainWindow::zoomToSelection()
{
    QList<QGraphicsItem *> selection = m_scene->selectedItems();
    if (selection.size() == 0)
    {
        QMessageBox::information(this, "No nodes selected", "You must first select nodes in the graph before using "
                                                            "the 'Zoom to fit selection' function.");
        return;
    }

    QRectF boundingBox;
    for (int i = 0; i < selection.size(); ++i)
    {
        QGraphicsItem * selectedItem = selection[i];
        boundingBox = boundingBox | selectedItem->boundingRect();
    }

    zoomToFitRect(boundingBox);
}



void MainWindow::selectContiguous()
{
    selectBasedOnContiguity(CONTIGUOUS_EITHER_STRAND);
}

void MainWindow::selectMaybeContiguous()
{
    selectBasedOnContiguity(MAYBE_CONTIGUOUS);
}

void MainWindow::selectNotContiguous()
{
    selectBasedOnContiguity(NOT_CONTIGUOUS);
}



void MainWindow::selectBasedOnContiguity(ContiguityStatus targetContiguityStatus)
{
    if (!g_assemblyGraph->m_contiguitySearchDone)
    {
        QMessageBox::information(this, "Contiguity determination not done",
                                       "To select nodes by their contiguity status, while in 'Colour "
                                       "by contiguity' mode, you must select a node and then click "
                                       "'Determine contiguity'.");
        return;
    }

    m_scene->blockSignals(true);
    m_scene->clearSelection();

    QMapIterator<QString, DeBruijnNode*> i(g_assemblyGraph->m_deBruijnGraphNodes);
    while (i.hasNext())
    {
        i.next();
        DeBruijnNode * node = i.value();
        GraphicsItemNode * graphicsItemNode = node->getGraphicsItemNode();

        if (graphicsItemNode == 0)
            continue;

        //For single nodes, choose the greatest contiguity status of this
        //node and its complement.
        ContiguityStatus nodeContiguityStatus = node->getContiguityStatus();
        if (!g_settings->doubleMode)
        {
            ContiguityStatus twinContiguityStatus = node->getReverseComplement()->getContiguityStatus();
            if (twinContiguityStatus < nodeContiguityStatus)
                nodeContiguityStatus = twinContiguityStatus;
        }

        if (targetContiguityStatus == CONTIGUOUS_EITHER_STRAND &&
                (nodeContiguityStatus == CONTIGUOUS_STRAND_SPECIFIC || nodeContiguityStatus == CONTIGUOUS_EITHER_STRAND))
            graphicsItemNode->setSelected(true);
        else if (targetContiguityStatus == MAYBE_CONTIGUOUS &&
                 nodeContiguityStatus == MAYBE_CONTIGUOUS)
            graphicsItemNode->setSelected(true);
        else if (targetContiguityStatus == NOT_CONTIGUOUS &&
                 nodeContiguityStatus == NOT_CONTIGUOUS)
            graphicsItemNode->setSelected(true);
    }

    m_scene->blockSignals(false);
    g_graphicsView->viewport()->update();
    selectionChanged();
    zoomToSelection();
}



void MainWindow::openBandageUrl()
{
    QDesktopServices::openUrl(QUrl("http://rrwick.github.io/Bandage/"));
}







void MainWindow::setWidgetsFromSettings()
{
    ui->singleNodesRadioButton->setChecked(!g_settings->doubleMode);
    ui->doubleNodesRadioButton->setChecked(g_settings->doubleMode);

    ui->nodeWidthSpinBox->setValue(g_settings->averageNodeWidth);

    ui->nodeNamesCheckBox->setChecked(g_settings->displayNodeNames);
    ui->nodeLengthsCheckBox->setChecked(g_settings->displayNodeLengths);
    ui->nodeReadDepthCheckBox->setChecked(g_settings->displayNodeReadDepth);
    ui->blastHitsCheckBox->setChecked(g_settings->displayBlastHits);
    ui->textOutlineCheckBox->setChecked(g_settings->textOutline);

    ui->startingNodesExactMatchRadioButton->setChecked(g_settings->startingNodesExactMatch);
    ui->startingNodesPartialMatchRadioButton->setChecked(!g_settings->startingNodesExactMatch);

    setNodeColourSchemeComboBox(g_settings->nodeColourScheme);

    setGraphScopeComboBox(g_settings->graphScope);
    ui->nodeDistanceSpinBox->setValue(g_settings->nodeDistance);
    ui->startingNodesLineEdit->setText(g_settings->startingNodes);
}



void MainWindow::setNodeColourSchemeComboBox(NodeColourScheme nodeColourScheme)
{
    switch (nodeColourScheme)
    {
    case RANDOM_COLOURS: ui->coloursComboBox->setCurrentIndex(0); break;
    case ONE_COLOUR: ui->coloursComboBox->setCurrentIndex(1); break;
    case READ_DEPTH_COLOUR: ui->coloursComboBox->setCurrentIndex(2); break;
    case BLAST_HITS_SOLID_COLOUR: ui->coloursComboBox->setCurrentIndex(3); break;
    case BLAST_HITS_RAINBOW_COLOUR: ui->coloursComboBox->setCurrentIndex(4); break;
    case CONTIGUITY_COLOUR: ui->coloursComboBox->setCurrentIndex(5); break;
    case CUSTOM_COLOURS: ui->coloursComboBox->setCurrentIndex(6); break;
    }
}

void MainWindow::setGraphScopeComboBox(GraphScope graphScope)
{
    switch (graphScope)
    {
    case WHOLE_GRAPH: ui->graphScopeComboBox->setCurrentIndex(0); break;
    case AROUND_NODE: ui->graphScopeComboBox->setCurrentIndex(1); break;
    case AROUND_BLAST_HITS: ui->graphScopeComboBox->setCurrentIndex(2); break;
    }
}

void MainWindow::nodeDistanceChanged()
{
    g_settings->nodeDistance = ui->nodeDistanceSpinBox->value();
}

void MainWindow::showEvent(QShowEvent *ev)
{
    QMainWindow::showEvent(ev);
    emit windowLoaded();
}


void MainWindow::startingNodesExactMatchChanged()
{
    g_settings->startingNodesExactMatch = ui->startingNodesExactMatchRadioButton->isChecked();
}


void MainWindow::openPathSpecifyDialog()
{
    //Don't open a second dialog if one's already up.
    if (g_memory->pathDialogIsVisible)
        return;

    PathSpecifyDialog * pathSpecifyDialog = new PathSpecifyDialog(this);
    connect(g_graphicsView, SIGNAL(doubleClickedNode(DeBruijnNode*)), pathSpecifyDialog, SLOT(addNodeName(DeBruijnNode*)));
    pathSpecifyDialog->show();
}


QString MainWindow::convertGraphFileTypeToString(GraphFileType graphFileType)
{
    QString graphFileTypeString;
    switch (graphFileType)
    {
    case LAST_GRAPH: graphFileTypeString = "LastGraph"; break;
    case FASTG: graphFileTypeString = "FASTG"; break;
    case GFA: graphFileTypeString = "GFA"; break;
    case TRINITY: graphFileTypeString = "Trinity.fasta"; break;
    case ANY_FILE_TYPE: graphFileTypeString = "any"; break;
    case UNKNOWN_FILE_TYPE: graphFileTypeString = "unknown"; break;
    }
    return graphFileTypeString;
}


void MainWindow::setSelectedNodesWidgetsVisibility(bool visible)
{
    ui->selectedNodesTitleLabel->setVisible(visible);
    ui->selectedNodesLine1->setVisible(visible);
    ui->selectedNodesLine2->setVisible(visible);
    ui->selectedNodesTextEdit->setVisible(visible);
    ui->selectedNodesModificationWidget->setVisible(visible);
    ui->selectedNodesLengthLabel->setVisible(visible);

    if (visible)
        ui->selectedNodesSpacer->changeSize(20, 60);
    else
        ui->selectedNodesSpacer->changeSize(20, 0);

}

void MainWindow::setSelectedEdgesWidgetsVisibility(bool visible)
{
    ui->selectedEdgesTitleLabel->setVisible(visible);
    ui->selectedEdgesTextEdit->setVisible(visible);
    ui->selectedEdgesLine->setVisible(visible);

    if (visible)
        ui->selectedEdgesSpacer->changeSize(20, 60);
    else
        ui->selectedEdgesSpacer->changeSize(20, 0);
}


void MainWindow::nodeWidthChanged()
{
    g_settings->averageNodeWidth = ui->nodeWidthSpinBox->value();
    g_assemblyGraph->recalculateAllNodeWidths();
    g_graphicsView->viewport()->update();
}


void MainWindow::fillBarcodeTable()
{

        ui->barcodeTable->clearContents();
        ui->barcodeTable->setSortingEnabled(false);

        int barcodeCount = g_barcode_manager->barcode_selected.size();
        ui->barcodeTable->setRowCount(barcodeCount);

        if (barcodeCount == 0)
            return;

        for (int i = 0; i < barcodeCount; ++i)
        {

            QTableWidgetItem * queryColour = new QTableWidgetItem(g_barcode_manager->presetColours.at(i).name().at(0));
            queryColour->setFlags(Qt::ItemIsEnabled);
            queryColour->setBackground(g_barcode_manager->presetColours.at(i));
            //
            //QTableWidgetItem * queryName = new QTableWidgetItem(hitQuery->getName());
            //queryName->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            //
            //QTableWidgetItem * nodeName = new QTableWidgetItem(hit->m_node->getName());
            //nodeName->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            //
            //TableWidgetItemDouble * percentIdentity = new TableWidgetItemDouble(QString::number(hit->m_percentIdentity) + "%", hit->m_percentIdentity);
            //percentIdentity->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            //
            //TableWidgetItemInt * alignmentLength = new TableWidgetItemInt(formatIntForDisplay(hit->m_alignmentLength), hit->m_alignmentLength);
            //alignmentLength->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            //
            //TableWidgetItemInt * numberMismatches = new TableWidgetItemInt(formatIntForDisplay(hit->m_numberMismatches), hit->m_numberMismatches);
            //numberMismatches->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            //
            //TableWidgetItemInt * numberGapOpens = new TableWidgetItemInt(formatIntForDisplay(hit->m_numberGapOpens), hit->m_numberGapOpens);
            //numberGapOpens->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            //
            //TableWidgetItemInt * queryStart = new TableWidgetItemInt(formatIntForDisplay(hit->m_queryStart), hit->m_queryStart);
            //queryStart->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            //
            //TableWidgetItemInt * queryEnd = new TableWidgetItemInt(formatIntForDisplay(hit->m_queryEnd), hit->m_queryEnd);
            //queryEnd->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            //
            //TableWidgetItemInt * nodeStart = new TableWidgetItemInt(formatIntForDisplay(hit->m_nodeStart), hit->m_nodeStart);
            //nodeStart->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            //
            //TableWidgetItemInt * nodeEnd = new TableWidgetItemInt(formatIntForDisplay(hit->m_nodeEnd), hit->m_nodeEnd);
            //nodeEnd->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            //
            //TableWidgetItemDouble * eValue = new TableWidgetItemDouble(QString::number(hit->m_eValue), hit->m_eValue);
            //eValue->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            //
            //TableWidgetItemDouble * bitScore = new TableWidgetItemDouble(QString::number(hit->m_bitScore), hit->m_bitScore);
            //bitScore->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            QString b = g_barcode_manager->barcode_selected.at(i);

            QTableWidgetItem * barcode = new QTableWidgetItem(g_barcode_manager->barcode_selected.at(i));
            QTableWidgetItem * barcode_count = new QTableWidgetItem(QString::number(g_barcode_manager->get_barcode_count(g_barcode_manager->barcode_selected.at(i))));
            ColourButton * colourButton = new ColourButton();

            if (g_barcode_manager->barcode_settings.contains(b))
            {
                //qDebug()<<"contains";
                BarcodeSetting* bs = g_barcode_manager->barcode_settings[b];
                colourButton->setColour(g_barcode_manager->presetColours.at(i));
                //qDebug()<<bs;
                //qDebug()<<bs->m_barcode << bs->m_active << bs->m_color;
                if (bs!=NULL){
                colourButton->setColour(bs->m_color);
                connect(colourButton, SIGNAL(colourChosen(QColor)), bs, SLOT(setColour(QColor)));
                connect(colourButton, SIGNAL(colourChosen(QColor)), this, SLOT(refreshDisplay()));




                }
            }
            else
                colourButton->setColour(g_barcode_manager->presetColours.at(i));



            QWidget * showCheckBoxWidget = new QWidget;
            QCheckBox * showCheckBox = new QCheckBox();
            QHBoxLayout * layout = new QHBoxLayout(showCheckBoxWidget);
            layout->addWidget(showCheckBox);
            layout->setAlignment(Qt::AlignCenter);
            layout->setContentsMargins(0, 0, 0, 0);
            showCheckBoxWidget->setLayout(layout);
            bool barcodeshown;
            if (g_barcode_manager->barcode_settings.contains(b))
            {
                BarcodeSetting* bs = g_barcode_manager->barcode_settings[b];
                barcodeshown = bs->m_active;
                connect(showCheckBox, SIGNAL(toggled(bool)), bs, SLOT(setShown(bool)));
                connect(showCheckBox, SIGNAL(toggled(bool)), this, SLOT(refreshDisplay()));
            }
            else
            {
                barcodeshown = false;
            }

            showCheckBox->setChecked(barcodeshown);
            QTableWidgetItem * show = new QTableWidgetItem(barcodeshown);
            show->setFlags(Qt::ItemIsEnabled);


            ui->barcodeTable ->setCellWidget(i,2,colourButton);
            ui->barcodeTable ->setItem(i, 1, barcode);
            ui->barcodeTable ->setItem(i,0, queryColour );
            ui->barcodeTable ->setItem(i, 3, barcode_count);
            ui->barcodeTable ->setCellWidget(i, 0, showCheckBoxWidget);
            ui->barcodeTable ->setItem(i, 0, show);

        }

        ui->barcodeTable ->resizeColumnsToContents();
        ui->barcodeTable->setEnabled(true);
        ui->barcodeTable->setSortingEnabled(true);
}

void MainWindow::setBarcodeSelectionNode(const QItemSelection &, const QItemSelection &){
    int foundNode = 0;
    //qDebug()<< "selection changed!";
    QModelIndexList selection = ui->barcodeTable->selectionModel()->selectedRows();
    QList<int>selected;
    for(int i=0; i< selection.count(); i++)
    {
        QModelIndex index = selection.at(i);
        //qDebug() << index.row();
        selected.append(index.row());
    }

    for (int i = 0; i < selected.size(); i++){
        QString barcode = g_barcode_manager->barcode_selected[selected[i]];
        if (g_barcode_manager->barocde_map.contains(barcode))
            for (int j = 0; j < g_barcode_manager->barocde_map[barcode].size(); j++){
                Barcode* current = g_barcode_manager->barocde_map[barcode].at(j);
                QString node;
                if (current->m_strand == 1)
                    node = current->m_contig + '-';
                else
                    node = current->m_contig + '+';

                if (g_assemblyGraph->m_deBruijnGraphNodes.contains(node))
                {
                    DeBruijnNode * db_node = g_assemblyGraph->m_deBruijnGraphNodes[node];
                    GraphicsItemNode* g_node = db_node->getGraphicsItemNode();
                    if (g_node!=0)
                    {
                        g_node->setSelected(true);
                        ++foundNode;
                    }
                }
            }
    }
    //qDebug()<<foundNode;
    if (foundNode > 0)
        zoomToSelection();
}


void MainWindow::refreshDisplay(){
    //qDebug()<<"refresh";
    for (int i = 0; i<g_barcode_manager->barcode_selected.size(); i++){
        QString b = g_barcode_manager->barcode_selected.at(i);
        if (g_barcode_manager->barocde_map.contains(b))
            //if (g_barcode_manager->barcode_settings.contains(b))
            //    if (g_barcode_manager->barcode_settings[b]->m_active)

            for (int j = 0; j<g_barcode_manager->barocde_map[b].size(); j++){
                Barcode* current = g_barcode_manager->barocde_map[b].at(j);
                /*if (g_assemblyGraph->m_deBruijnGraphNodes.contains(current->m_contig + '+')){
                    g_assemblyGraph->m_deBruijnGraphNodes[current->m_contig + '+']->addBarcode(current);
                    current->setColor(g_barcode_manager->presetColours.at(i));
                    //qDebug() << current->m_contig + '+';
                */

                if (current->m_strand == 0) // not reverse
                {
                    if (g_assemblyGraph->m_deBruijnGraphNodes.contains(current->m_contig + '+')){
                                        g_assemblyGraph->m_deBruijnGraphNodes[current->m_contig + '+']->addBarcode(current);

                                        if (g_barcode_manager->barcode_settings.contains(b)){
                                            current->setColor(g_barcode_manager->barcode_settings[b]->m_color);
                                        }
                                        else
                                            current->setColor(g_barcode_manager->presetColours.at(i));
                    }
                }
                else // reverse
                {
                    if (g_assemblyGraph->m_deBruijnGraphNodes.contains(current->m_contig + '-')){
                                        g_assemblyGraph->m_deBruijnGraphNodes[current->m_contig + '-']->addBarcode(current);
                                        if (g_barcode_manager->barcode_settings.contains(b)){
                                            current->setColor(g_barcode_manager->barcode_settings[b]->m_color);
                                        }
                                        else
                                            current->setColor(g_barcode_manager->presetColours.at(i));
                    }
                }

            }
    }


    m_scene->blockSignals(false);
    g_graphicsView->viewport()->update();




}
