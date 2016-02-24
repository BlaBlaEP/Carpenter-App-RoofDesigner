#include "sketchstaticsexporter.h"
#include <QDebug>
#include <QDir>
#include <QImage>
#include "globals.h"
SketchStaticsExporter::SketchStaticsExporter(QObject *parent) : QObject(parent) {
    this->sketch = Q_NULLPTR;
    for(int i = 0; i < 26; i++) {
        this->idToLetter[i] = i + 65;
    }
}

void SketchStaticsExporter::setSketch(QObject *sketch) {
    this->sketch = sketch;
}

QObject* SketchStaticsExporter::getSketch() {
    return this->sketch;
}

bool SketchStaticsExporter::identifierToLetter(int id, QString &name) {
    if(id < 0) {
        return false;
    }
    // would be amazing to have so much point
    if(id > 1000) {
        return false;
    }

    // so we won't have overflow here
    int subscript = (id + 1)/26;
    int letterIndex = id % 26;
    if(letterIndex < 0) {
        return false;
    }

    name.append(this->idToLetter[id % 26]);

    if(subscript != 0) {
        name.append(QString::number(subscript));
    }

    return true;
}

QVariant SketchStaticsExporter::exportToFile(QString basename, QString backgroundImagePath, QString path) {
    if(this->sketch == Q_NULLPTR) {
        return "No sketch to export";
    }

    QVariant mmPerPixelScale;
    bool isGetMmPerPixelScaleCallWorks = QMetaObject::invokeMethod(
                sketch,
                "getMmPerPixelScale",
                Q_RETURN_ARG(QVariant, mmPerPixelScale)
    );

    if(mmPerPixelScale==0){
         return "Set Scale First";
    }


    if(basename.isEmpty()){
        return "Invalid basename";
    }

    if(path.isEmpty()){
        path=scenariosDir+basename+"/";
    }

    if(!QDir(path).exists()){
        if(!QDir().mkpath(path))
            return "Can't create path for exportFile";
    }

    QString filename=path+basename+".static_structure";
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) ){
        return "Cannot open the model file to export";
    }

    QVariantMap store = this->sketch->property("store").value<QVariantMap>();
    QVariantList points = store["points"].value<QVariantList>();
    QVariantList lines = store["lines"].value<QVariantList>();

    QMap<int, int> pointToStaticsIndex;

    int numberOfPoints = points.size();
    int numberOfReaction = 0;
    int numberOfBeams = lines.size();


    QTextStream stream( &file );

    stream << "#Scale factor between tangible model and real model(affect only the position of the joints)" << endl
           << "0.1" << endl
           << endl;

    stream << "#Number of nodes" << endl
           << numberOfPoints << endl
           << endl;

    stream << "#Node id,x,y,z,dim(always 0)" << endl;

    int i = 0;



    foreach(QVariant rawPoint, points) {
        QObject* point = rawPoint.value<QObject*>();
        int identifier = point->property("identifier").toInt();
        QVector2D position = point->property("start").value<QVector2D>();

        pointToStaticsIndex[identifier] = i;

        stream << i << ","
               << position.x()*mmPerPixelScale.toDouble() << ","
               << position.y()*mmPerPixelScale.toDouble() << ","
               << 0 << ","
               << 0
               << endl;
        i++;
    }

    stream << endl;




    QString reactions;
    QTextStream reactionsStream(&reactions, QIODevice::WriteOnly);
    i = 0;
    foreach(QVariant rawPoint, points) {
        QObject* point = rawPoint.value<QObject*>();

        bool cx = point->property("cx").toBool();
        bool cy = point->property("cy").toBool();
        bool cz = point->property("cz").toBool();

        bool mx = point->property("mx").toBool();
        bool my = point->property("my").toBool();
        bool mz = point->property("mz").toBool();

        if(cx || cy || cz || mx || my || mz) {
            numberOfReaction++;

            reactionsStream << i << ","
                            << (cx ? "1" : "0") << ","
                            << (cy ? "1" : "0") << ","
                            << (cz ? "1" : "0") << ","
                            << (mx ? "1" : "0") << ","
                            << (my ? "1" : "0") << ","
                            << (mz ? "1" : "0")
                            << endl;
        }
        i++;
    }

    stream << "#Number of reaction" << endl
           << numberOfReaction << endl
           << endl;

    stream << "#Node Id,x,y,z,Mx,My,Mz" << endl
           << reactions
           << endl;

    stream << "#Number of beams" << endl
           << numberOfBeams << endl
           << endl;

    stream << "#Beam name,node 1,node 2,width,height,Youngh,E,d" << endl;

    foreach(QVariant rawLine, lines) {
        QObject* line = rawLine.value<QObject*>();
        QObject* start = line->property("startPoint").value<QObject*>();
        QObject* end = line->property("endPoint").value<QObject*>();
        int idStart = start->property("identifier").toInt();
        int idEnd = end->property("identifier").toInt();
        QVector2D pointer = line->property("pointer").value<QVector2D>();

        QString letterStart;
        QString letterEnd;

        if(
                !this->identifierToLetter(pointToStaticsIndex[idStart], letterStart)
                || !this->identifierToLetter(pointToStaticsIndex[idEnd], letterEnd)
        ) {
            file.close();
            return "Unable to transform identifier to export format";
        }

        stream << letterStart << letterEnd << ","
               << pointToStaticsIndex[idStart] << ","
               << pointToStaticsIndex[idEnd] << ","
               << pointer.length() << ","
               << (SketchLine::radius * 2) << ","
               << "20000" << ","
               << "1250" << ","
               << "0.50e-9"
               << endl;
    }

    stream << endl;

    QImage backgroundImage(backgroundImagePath);
    if(!backgroundImage.isNull()){
        backgroundImage.save(path+basename+".png");
    }
    else
        qDebug()<<"Null background image";



    return true;
}
