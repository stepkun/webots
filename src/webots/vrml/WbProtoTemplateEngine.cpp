// Copyright 1996-2021 Cyberbotics Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "WbProtoTemplateEngine.hpp"

#include "WbApplicationInfo.hpp"
#include "WbStandardPaths.hpp"

#include "WbField.hpp"
#include "WbMultipleValue.hpp"
#include "WbNode.hpp"
#include "WbNodeModel.hpp"
#include "WbProject.hpp"
#include "WbProtoModel.hpp"
#include "WbRgb.hpp"
#include "WbRotation.hpp"
#include "WbSingleValue.hpp"
#include "WbVariant.hpp"
#include "WbVector2.hpp"
#include "WbVector3.hpp"
#include "WbVersion.hpp"

#include <QtCore/QVector>

#include <cassert>

static QString gCoordinateSystem;

WbProtoTemplateEngine::WbProtoTemplateEngine(const QString &templateContent, const QString &engine) :
  WbTemplateEngine(templateContent, engine) {
}

bool WbProtoTemplateEngine::generate(const QString &logHeaderName, const QVector<WbField *> &parameters,
                                     const QString &protoPath, const QString &worldPath, int id) {
  // generate the final script file from the template script file
  printf("WbProtoTemplateEngine::generate (param size: %d)\n", parameters.size());
  QHash<QString, QString> tags;

  tags["fields"] = scriptingEngine() == "javascript" ? "var fields = {" : "";
  foreach (const WbField *parameter, parameters) {
    printf(">> parameter: %s (isTemplateRegenerator: %d)\n", parameter->name().toUtf8().constData(),
           parameter->isTemplateRegenerator());
    if (!parameter->isTemplateRegenerator())  // keep only regenerator fields
      continue;
    const QString &valueString = convertFieldValueToStatement(parameter, scriptingEngine());
    if (!valueString.isEmpty()) {
      tags["fields"] += QString("%1 = {").arg(parameter->name());
      tags["fields"] += QString("value = %1, ").arg(valueString);
      tags["fields"] += QString("defaultValue = %1").arg(convertFieldDefaultValueToStatement(parameter, scriptingEngine()));
    }
    tags["fields"] += "},\n";
  }
  tags["fields"].chop(2);  // remove the last ",\n" if any
  if (scriptingEngine() == "javascript")
    tags["fields"] += "}";  // close object

  tags["context"] = scriptingEngine() == "javascript" ? "var context = {" : "";
#ifdef _WIN32
  tags["context"] += QString("os = \"windows\",");
#endif
#ifdef __linux__
  tags["context"] += QString("os = \"linux\",");
#endif
#ifdef __APPLE__
  tags["context"] += QString("os = \"mac\",");
#endif
  tags["context"] += QString("world = \"%1\", ").arg(worldPath);
  tags["context"] += QString("proto = \"%1\",").arg(protoPath);
  tags["context"] += QString("webots_home = \"%1\",").arg(WbStandardPaths::webotsHomePath());
  tags["context"] += QString("project_path = \"%1\",").arg(WbProject::current()->path());
  tags["context"] += QString("temporary_files_path = \"%1\",").arg(WbStandardPaths::webotsTmpPath());
  tags["context"] += QString("id = \"%1\",").arg(id);
  tags["context"] += QString("coordinate_system = \"%1\",").arg(gCoordinateSystem);
  WbVersion version = WbApplicationInfo::version();
  // for example major = R2018a and revision = 0
  tags["context"] += QString("webots_version = { major = \"%1\", revision = \"%2\" }")
                       .arg(version.toString(false))
                       .arg(version.revisionNumber());

  if (scriptingEngine() == "javascript")
    tags["context"] += "}";  // close object

  printf("Scipting Language: %s\n", scriptingEngine().toUtf8().constData());
  return WbTemplateEngine::generate(tags, logHeaderName);
}

void WbProtoTemplateEngine::setCoordinateSystem(const QString &coordinateSystem) {
  gCoordinateSystem = coordinateSystem;
}

const QString &WbProtoTemplateEngine::coordinateSystem() {
  return gCoordinateSystem;
}

QString WbProtoTemplateEngine::convertFieldValueToStatement(const WbField *field, const QString &templateEngine) {
  if (field->isSingle()) {
    const WbSingleValue *singleValue = dynamic_cast<const WbSingleValue *>(field->value());
    assert(singleValue);
    const WbVariant &variant = singleValue->variantValue();
    return convertVariantToStatement(variant, templateEngine);
  } else if (field->isMultiple()) {
    const WbMultipleValue *multipleValue = dynamic_cast<const WbMultipleValue *>(field->value());
    assert(multipleValue);
    // multiple values into a lua array
    QString result = "{";
    for (int i = 0; i < multipleValue->size(); ++i) {
      if (i != 0)
        result += ", ";
      const WbVariant &variant = multipleValue->variantValue(i);
      result += convertVariantToStatement(variant, templateEngine);
    }
    result += "}";
    return result;
  }

  assert(false);
  return "";
}

QString WbProtoTemplateEngine::convertFieldDefaultValueToStatement(const WbField *field, const QString &templateEngine) {
  if (field->isSingle()) {
    const WbSingleValue *singleValue = dynamic_cast<const WbSingleValue *>(field->defaultValue());
    assert(singleValue);
    const WbVariant &variant = singleValue->variantValue();
    return convertVariantToStatement(variant, templateEngine);
  }

  else if (field->isMultiple()) {
    const WbMultipleValue *multipleValue = dynamic_cast<const WbMultipleValue *>(field->defaultValue());
    assert(multipleValue);
    // multiple values into a lua array
    QString result = "{";
    for (int i = 0; i < multipleValue->size(); ++i) {
      if (i != 0)
        result += ", ";
      const WbVariant &variant = multipleValue->variantValue(i);
      result += convertVariantToStatement(variant, templateEngine);
    }
    result += "}";
    return result;
  }

  assert(false);
  return "";
}

QString WbProtoTemplateEngine::convertVariantToStatement(const WbVariant &variant, const QString &templateEngine) {
  switch (variant.type()) {
    case WB_SF_BOOL:
      return QString("%1").arg(variant.toBool() ? "true" : "false");
    case WB_SF_INT32:
      return QString("%1").arg(variant.toInt());
    case WB_SF_FLOAT:
      return QString("%1").arg(variant.toDouble());
    case WB_SF_VEC2F: {
      QString statement = QString("{x: %1, y: %2}").arg(variant.toVector2().x()).arg(variant.toVector2().y());
      return templateEngine == "javascript" ? statement : statement.replace(":", " =");
    }
    case WB_SF_VEC3F: {
      QString statement =
        QString("{x: %1, y: %2, z: %3}").arg(variant.toVector3().x()).arg(variant.toVector3().y()).arg(variant.toVector3().z());
      return templateEngine == "javascript" ? statement : statement.replace(":", " =");
    }
    case WB_SF_ROTATION: {
      QString statement = QString("{x: %1, y: %2, z: %3, a: %4}")
                            .arg(variant.toRotation().x())
                            .arg(variant.toRotation().y())
                            .arg(variant.toRotation().z())
                            .arg(variant.toRotation().angle());
      return templateEngine == "javascript" ? statement : statement.replace(":", " =");
    }
    case WB_SF_COLOR: {
      QString statement = QString("{r: %1, g: %2, b: %3}")
                            .arg(variant.toColor().red())
                            .arg(variant.toColor().green())
                            .arg(variant.toColor().blue());
      return templateEngine == "javascript" ? statement : statement.replace(":", " =");
    }
    case WB_SF_STRING: {
      QString string = variant.toString();
      string.replace("\\\"", "\"");  // make sure that if a double
      // quote is already protected we don't 'break' this potection
      string.replace("\"", "\\\"");
      return QString("\"%1\"").arg(string);
    }
    case WB_SF_NODE: {
      WbNode *node = variant.toNode();
      if (node) {
        // lua dictionary
        QString nodeString = "{";

        // node_name key
        nodeString += QString("node_name = \"%1\"").arg(node->modelName());
        nodeString += ", ";

        // fields key: fieldName = fieldValue
        nodeString += "fields = {";
        foreach (const WbField *field, node->fieldsOrParameters()) {
          if (field->name() != "node_name") {
            nodeString += QString("%1 = {").arg(field->name());
            nodeString += QString("value = %1, ").arg(convertFieldValueToStatement(field, templateEngine));
            nodeString += QString("defaultValue = %1").arg(convertFieldDefaultValueToStatement(field, templateEngine));
            if (field != node->fieldsOrParameters().last())
              nodeString += "}, ";
            else
              nodeString += "}";
          }
        }
        nodeString += "}";  // end of fields

        nodeString += "}";  // end of nodes

        return nodeString;
      } else
        return "nil";
    }
    default:
      assert(false);
      return "";
  }
}
