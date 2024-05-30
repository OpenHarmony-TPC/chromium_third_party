/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "third_party/blink/renderer/core/ohos_adblock/ohos_adblock_util.h"

#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

bool IsValidTarget(Node* context, int top, int left, int width, int height) {
  if (!context || !context->parentNode()) {
    return false;
  }

  Element* target = To<Element>(context);
  if (!target) {
    return false;
  }

  int t_top = target->OffsetTop();
  int t_left = target->OffsetLeft();
  int t_height = target->OffsetHeight();
  int t_width = target->OffsetWidth();

  if (context->parentNode()->getNodeType() ==
          blink::Node::NodeType::kDocumentNode ||
      context->parentNode()->nodeName().DeprecatedLower().Utf8() == "body") {
    return false;
  }

  NodeList* siblings = context->parentNode()->childNodes();
  if (siblings && siblings->length() > 1) {
    for (unsigned int i = 0; siblings && i < siblings->length(); ++i) {
      Node* sibling = siblings->item(i);
      if (sibling->isSameNode(context)) {
        continue;
      }
      if (To<Element>(sibling)->OffsetHeight() > 0 &&
          To<Element>(sibling)->OffsetWidth() > 0) {
        return false;
      }
    }
  }

  if (top <= t_top && left <= t_left && (top + height) >= (t_top + t_height) &&
      (left + width) >= (t_left + t_width)) {
    return true;
  }
  return false;
}

std::string Escape(const std::string& ident) {
  StringBuilder builder;
  blink::SerializeIdentifier(
      WTF::String::FromUTF8(ident.data(), ident.length()), builder);
  return builder.ToString().Utf8();
}

std::vector<std::string> PrefixedElementClassNames(Node& node,
                                                   bool pureDomPath) {
  if (pureDomPath) {
    return std::vector<std::string>();
  }

  Element* element = To<Element>(&node);
  if (element) {
    return std::vector<std::string>();
  }

  std::string classAttribute =
      element->getAttribute(html_names::kClassAttr).Utf8();
  if (classAttribute.empty()) {
    return std::vector<std::string>();
  }

  std::stringstream ss(classAttribute);
  std::string className;
  std::vector<std::string> results;

  while (ss >> className) {
    if (!className.empty()) {
      results.push_back("$" + className);
    }
  }
  return results;
}

std::pair<std::string, bool> GetDomPathStep(Node& node,
                                            bool optimized,
                                            bool isTargetNode,
                                            bool pureDomPath) {
  if (node.getNodeType() != blink::Node::NodeType::kElementNode) {
    return std::make_pair("", false);
  }

  Element* element = To<Element>(&node);
  if (!element) {
    return std::make_pair("", false);
  }

  std::string id = element->getAttribute(html_names::kIdAttr).Utf8();
  std::string nodeNameInCorrectCase =
      (!node.GetShadowRoot() || node.GetShadowRoot()->mode().Utf8().empty())
          ? node.nodeName().Utf8()
          : "#shadow-root (" + node.GetShadowRoot()->mode().Utf8() + ")";

  if (optimized) {
    if (!id.empty()) {
      return std::make_pair("#" + Escape(id), true);
    }
    std::string nodeNameLower = node.nodeName().DeprecatedLower().Utf8();
    if (nodeNameLower == "body" || nodeNameLower == "head" ||
        nodeNameLower == "html") {
      return std::make_pair(nodeNameInCorrectCase, true);
    }
  }

  std::string nodeName = nodeNameInCorrectCase;

  if (!id.empty() && !pureDomPath) {
    return std::make_pair(nodeName + "#" + Escape(id), true);
  }

  Node* parent = node.parentNode();
  if (!parent ||
      parent->getNodeType() == blink::Node::NodeType::kDocumentNode) {
    return std::make_pair(nodeName, true);
  }

  std::vector<std::string> prefixedOwnClassNamesArray =
      PrefixedElementClassNames(node, pureDomPath);
  bool needsClassNames = false;
  bool needsNthChild = false;
  int ownIndex = -1;
  int elementIndex = -1;
  NodeList* siblings = parent->childNodes();

  for (unsigned int i = 0;
       siblings && (ownIndex == -1 || !needsNthChild) && i < siblings->length();
       ++i) {
    Node* sibling = siblings->item(i);
    if (sibling->getNodeType() != blink::Node::NodeType::kElementNode) {
      continue;
    }

    elementIndex += 1;
    if (sibling->isSameNode(&node)) {
      ownIndex = elementIndex;
      continue;
    }

    if (needsNthChild) {
      continue;
    }

    std::string siblingNodeNameInCorrectCase =
        (!sibling->GetShadowRoot() ||
         sibling->GetShadowRoot()->mode().Utf8().empty())
            ? sibling->nodeName().Utf8()
            : "#shadow-root (" + sibling->GetShadowRoot()->mode().Utf8() + ")";

    if (siblingNodeNameInCorrectCase != nodeName) {
      continue;
    }

    needsClassNames = true;
    std::vector<std::string> ownClassNames = prefixedOwnClassNamesArray;
    if (ownClassNames.size() == 0) {
      needsNthChild = true;
      continue;
    }

    std::vector<std::string> siblingClassNamesArray =
        PrefixedElementClassNames(*sibling, pureDomPath);
    for (unsigned int j = 0; j < siblingClassNamesArray.size(); ++j) {
      std::string siblingClass = siblingClassNamesArray[j];
      std::vector<std::string>::iterator it =
          find(ownClassNames.begin(), ownClassNames.end(), siblingClass);
      if (it == ownClassNames.end()) {
        continue;
      }
      ownClassNames.erase(it);
      if (ownClassNames.size() == 0) {
        needsNthChild = true;
        break;
      }
    }
  }

  std::string result = nodeName;
  std::string lowerCaseNodeName = nodeName;
  std::transform(lowerCaseNodeName.begin(), lowerCaseNodeName.end(),
                 lowerCaseNodeName.begin(), ::tolower);

  if (isTargetNode && lowerCaseNodeName == "input" &&
      !element->getAttribute(html_names::kTypeAttr).empty() &&
      element->getAttribute(html_names::kIdAttr).empty() &&
      element->getAttribute(html_names::kClassAttr).empty()) {
    result += "[type=" +
              Escape((element->getAttribute(html_names::kTypeAttr)).Utf8()) +
              "]";
  }

  if (needsNthChild) {
    result += ":nth-child(" + base::NumberToString(ownIndex + 1) + ")";
  } else if (needsClassNames) {
    for (auto prefixedName : prefixedOwnClassNamesArray) {
      result += "." + Escape(prefixedName.substr(1));
    }
  }
  return std::make_pair(result, false);
}

std::string GetDomPath(Node& node, bool optimized, bool pureDomPath) {
  if (node.getNodeType() != blink::Node::NodeType::kElementNode) {
    return "";
  }
  std::vector<std::string> steps;
  std::string result;
  Node* contextNode = &node;
  while (contextNode) {
    std::pair<std::string, bool> step = GetDomPathStep(
        *contextNode, optimized, node.isSameNode(contextNode), pureDomPath);
    if (step.first.empty()) {
      break;
    }
    steps.push_back(step.first);
    if (step.second) {
      break;
    }
    contextNode = contextNode->parentNode();
  }
  for (long i = steps.size() - 1; i >= 0; i--) {
    result += steps[i];
    if (i != 0) {
      result += ">";
    }
  }
  return result;
}
}  // namespace blink