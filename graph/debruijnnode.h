//Copyright 2015 Ryan Wick

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


#ifndef DEBRUIJNNODE_H
#define DEBRUIJNNODE_H

#include <QByteArray>
#include <vector>
#include <ogdf/basic/Graph.h>
#include "../program/globals.h"
#include <QColor>
#include "../blast/blasthitpart.h"
#include "barcode.h"

class OgdfNode;
class DeBruijnEdge;
class GraphicsItemNode;
class BlastHit;

class DeBruijnNode
{
public:
    //CREATORS
    DeBruijnNode(QString name, double readDepth, QByteArray sequence);
    ~DeBruijnNode();

    //ACCESSORS
    QString getName() const {return m_name;}
    double getReadDepth() const {return m_readDepth;}
    double getReadDepthRelativeToMeanDrawnReadDepth() const {return m_readDepthRelativeToMeanDrawnReadDepth;}
    QByteArray getSequence() const {return m_sequence;}
    int getLength() const {return m_sequence.length();}
    QByteArray getFasta() const;
    char getBaseAt(int i) const {if (i >= 0 && i < m_sequence.length()) return m_sequence.at(i); else return '\0';}
    ContiguityStatus getContiguityStatus() const {return m_contiguityStatus;}
    DeBruijnNode * getReverseComplement() const {return m_reverseComplement;}
    OgdfNode * getOgdfNode() const {return m_ogdfNode;}
    GraphicsItemNode * getGraphicsItemNode() const {return m_graphicsItemNode;}
    bool hasGraphicsItem() const {return m_graphicsItemNode != 0;}
    const std::vector<DeBruijnEdge *> * getEdgesPointer() const {return &m_edges;}
    std::vector<DeBruijnEdge *> getEnteringEdges() const;
    std::vector<DeBruijnEdge *> getLeavingEdges() const;
    std::vector<DeBruijnNode *> getDownstreamNodes() const;
    std::vector<DeBruijnNode *> getUpstreamNodes() const;
    bool isSpecialNode() const {return m_specialNode;}
    bool isDrawn() const {return m_drawn;}
    bool isNotDrawn() const {return !m_drawn;}
    QColor getCustomColour() const {return m_customColour;}
    QString getCustomLabel() const {return m_customLabel;}
    bool isPositiveNode() const;
    bool isNegativeNode() const;
    bool inOgdf() const {return m_ogdfNode != 0;}
    bool notInOgdf() const {return m_ogdfNode == 0;}
    bool thisOrReverseComplementInOgdf() const {return (inOgdf() || getReverseComplement()->inOgdf());}
    bool thisOrReverseComplementNotInOgdf() const {return !thisOrReverseComplementInOgdf();}
    bool isNodeConnected(DeBruijnNode * node) const;
    const std::vector<BlastHit *> * getBlastHitsPointer() const {return &m_blastHits;}
    bool thisNodeHasBlastHits() const {return m_blastHits.size() > 0;}
    bool thisNodeHasBarcode() const {return m_barcodes.size() > 0;}

    bool thisNodeOrReverseComplementHasBlastHits() const {return m_blastHits.size() > 0 || getReverseComplement()->m_blastHits.size() > 0;}
    bool thisNodeOrReverseComplementHasBarcode() const {return m_barcodes.size() > 0 || getReverseComplement()->m_barcodes.size() > 0;}

    DeBruijnEdge * doesNodeLeadIn(DeBruijnNode * node) const;
    DeBruijnEdge * doesNodeLeadAway(DeBruijnNode * node) const;
    std::vector<BlastHitPart> getBlastHitPartsForThisNode(double scaledNodeLength) const;
    std::vector<BarcodePart> getBarcodePartsForThisNode(double scaledNodeLength) const;

    std::vector<BlastHitPart> getBlastHitPartsForThisNodeOrReverseComplement(double scaledNodeLength) const;
    std::vector<BarcodePart> getBarcodePartsForThisNodeOrReverseComplement(double scaledNodeLength) const;


    //MODIFERS
    void setReadDepthRelativeToMeanDrawnReadDepth(double newVal) {m_readDepthRelativeToMeanDrawnReadDepth = newVal;}
    void appendToSequence(QByteArray additionalSeq) {m_sequence.append(additionalSeq);}
    void upgradeContiguityStatus(ContiguityStatus newStatus);
    void resetContiguityStatus() {m_contiguityStatus = NOT_CONTIGUOUS;}
    void setReverseComplement(DeBruijnNode * rc) {m_reverseComplement = rc;}
    void setGraphicsItemNode(GraphicsItemNode * gin) {m_graphicsItemNode = gin;}
    void setAsSpecial() {m_specialNode = true;}
    void setAsNotSpecial() {m_specialNode = false;}
    void setAsDrawn() {m_drawn = true;}
    void setAsNotDrawn() {m_drawn = false;}
    void setCustomColour(QColor newColour) {m_customColour = newColour;}
    void setCustomLabel(QString newLabel) {m_customLabel = newLabel;}
    void resetNode();
    void addEdge(DeBruijnEdge * edge);
    void addToOgdfGraph(ogdf::Graph * ogdfGraph);
    void determineContiguity();
    void clearBlastHits() {m_blastHits.clear();}
    void addBlastHit(BlastHit * newHit) {m_blastHits.push_back(newHit);}
    void labelNeighbouringNodesAsDrawn(int nodeDistance, DeBruijnNode * callingNode);
    void addBarcode(Barcode * bc) { m_barcodes.push_back(bc) ;}

private:
    QString m_name;
    double m_readDepth;
    double m_readDepthRelativeToMeanDrawnReadDepth;
    QByteArray m_sequence;
    ContiguityStatus m_contiguityStatus;
    DeBruijnNode * m_reverseComplement;
    OgdfNode * m_ogdfNode;
    GraphicsItemNode * m_graphicsItemNode;
    std::vector<DeBruijnEdge *> m_edges;
    bool m_specialNode;
    bool m_drawn;
    int m_highestDistanceInNeighbourSearch;
    QColor m_customColour;
    QString m_customLabel;
    std::vector<BlastHit *> m_blastHits;

    std::vector<Barcode *> m_barcodes;

    int getBasePairsPerSegment() const;
    bool isOnlyPathInItsDirection(DeBruijnNode * connectedNode,
                                  std::vector<DeBruijnNode *> * incomingNodes,
                                  std::vector<DeBruijnNode *> * outgoingNodes) const;
    bool isNotOnlyPathInItsDirection(DeBruijnNode * connectedNode,
                                     std::vector<DeBruijnNode *> * incomingNodes,
                                     std::vector<DeBruijnNode *> * outgoingNodes) const;
    std::vector<DeBruijnNode *> getNodesCommonToAllPaths(std::vector< std::vector <DeBruijnNode *> > * paths,
                                                         bool includeReverseComplements) const;
    bool doesPathLeadOnlyToNode(DeBruijnNode * node, bool includeReverseComplement);
};

#endif // DEBRUIJNNODE_H
