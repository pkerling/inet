//
// Copyright (C) 2005 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "ScenarioManager.h"
#include "util/opp_utils.h"

Define_Module(ScenarioManager);


void ScenarioManager::initialize()
{
    cXMLElement *script = par("script");

    numChanges = numDone = 0;
    WATCH(numChanges);
    WATCH(numDone);

    for (cXMLElement *node=script->getFirstChild(); node; node = node->getNextSibling())
    {
        simtime_t t = STR_SIMTIME(attributeOrError(node, "t"));

        if (!strcmp(node->getTagName(), "interpolate"))
        {
            // Calculate number of changes
            simtime_t until    = STR_SIMTIME(attributeOrError(node, "until"));
            simtime_t interval = STR_SIMTIME(attributeOrError(node, "interval"));
            simtime_t duration = until - t;

            if (until < t)
                error("interpolation end is before start at %s", node->getSourceLocation());

            numChanges += static_cast<int> (floor(duration.dbl() / interval.dbl()));

            cXMLElement* commandsNode = node->getFirstChildWithTag("commands");
            if (!commandsNode)
                error("interpolation has no commands set at %s", node->getSourceLocation());

            Interpolation *interpolation = new Interpolation;
            interpolation->node = node;
            interpolation->commandsNode = commandsNode;
            interpolation->interval = interval;
            interpolation->currentTargetNode = 0;
            interpolation->currentValue = 0;
            interpolation->currentStep = 0.0;
            interpolation->currentStepsDone = interpolation->currentStepsTotal = 0;
#if 0
            int childNo = 0;
            simtime_t lastT = t;
            for (cXMLElement *interpolateChild=node->getFirstChild(); interpolateChild; interpolateChild=interpolateChild->getNextSibling())
            {
                simtime_t childT = STR_SIMTIME(attributeOrError(interpolateChild, "t"));

                if (childT < t)
                    error("node t is before interpolation start at %s", interpolateChild->getSourceLocation());

                if (childT < lastT)
                    error("node t is before previous node t at %s", interpolateChild->getSourceLocation());
                lastT = childT;

                if (childT > until)
                    error("node t is after interpolation end at %s", interpolateChild->getSourceLocation());

                if (childNo == 0)
                {
                    interpolation->currentValue = attributeOrError(interpolateChild, "value");
                }
                else if (childNo == 1)
                {
                    interpolation->currentTarget = attributeOrError(interpolateChild, "value");
                    interpolation->lastScheduledInvocation = childT;
                }

                if (childT != t)
                {
                    // schedule self-message if this does not match the beginning
                    cMessage *msg = new cMessage("interpolation-change", SCENARIO_MESSAGE_INTERPOLATION_CHANGE);
                    msg->setContextPointer(interpolateChild);
                    scheduleAt(childT, msg);
                }

                childNo++;
            }

            if (childNo < 2)
                error("too few interpolation values, need at least two child nodes at %s", node->getSourceLocation());
#endif
            // schedule self-message
            cMessage *msg = new cMessage("interpolation-start", SCENARIO_MESSAGE_INTERPOLATION_CHANGE);
            msg->setContextPointer(interpolation);
            scheduleAt(t, msg);
        }
        else
        {
            // schedule self-message
            cMessage *msg = new cMessage("scenario-event");
            msg->setContextPointer(node);
            scheduleAt(t, msg);

            // count it
            numChanges++;
        }
    }

    updateDisplayString();
}

const char *ScenarioManager::attributeOrError(cXMLElement *node, const char *attributeName)
{
    const char *attr = node->getAttribute(attributeName);
    if (!attr)
        error("attribute '%s' missing at %s", attributeName, node->getSourceLocation());

    return attr;
}

void ScenarioManager::handleMessage(cMessage *msg)
{
    if (msg->getKind() == SCENARIO_MESSAGE_INTERPOLATION_CHANGE)
    {
        Interpolation *interpolation = static_cast<Interpolation*> (msg->getContextPointer());
        processInterpolationChange(interpolation);
    }
    else if (msg->getKind() == SCENARIO_MESSAGE_INTERPOLATION_UPDATE)
    {
        Interpolation *interpolation = static_cast<Interpolation*> (msg->getContextPointer());
        processInterpolationUpdate(interpolation);
    }
    else
    {
        cXMLElement *node = (cXMLElement *) msg->getContextPointer();
        processCommand(node);
    }

    delete msg;

    numDone++;
    updateDisplayString();
}

void ScenarioManager::processInterpolationChange(Interpolation *interpolation)
{
    bool isFirst = !interpolation->currentTargetNode;
    double oldTarget = interpolation->currentTarget;

    // If this is the first iteration, we do not have a target yet and need to set it to the first one
    if (isFirst)
    {
        EV << "Starting interpolation\n";
        cXMLElement* valuesNode = interpolation->node->getFirstChildWithTag("values");
        if (!valuesNode)
            error("interpolation has no values set at %s", interpolation->node->getSourceLocation());

        cXMLElement* firstValueNode = valuesNode->getFirstChild();
        if (!firstValueNode)
            error("interpolation has no values set at %s", valuesNode->getSourceLocation());
        if (strcmp(firstValueNode->getTagName(), "at") != 0)
            error("interpolation values child node is not an 'at' node at %s", firstValueNode->getSourceLocation());

        interpolation->currentTargetNode = firstValueNode;
    }
    else
    {
        // Otherwise use the next sibling node
        interpolation->currentTargetNode = interpolation->currentTargetNode->getNextSibling();
    }

    // Calculate new target time
    simtime_t nextT;
    if (interpolation->currentTargetNode)
    {
        if (strcmp(interpolation->currentTargetNode->getTagName(), "at") != 0)
            error("interpolation values child node is not an 'at' node at %s", interpolation->currentTargetNode->getSourceLocation());

        interpolation->currentTarget = OPP_Global::atod(attributeOrError(interpolation->currentTargetNode, "value"));
        nextT = STR_SIMTIME(attributeOrError(interpolation->currentTargetNode, "t"));

        if (nextT <= simTime())
            error("next interpolation value point time is before current value point at %s", interpolation->currentTargetNode->getSourceLocation());
    }
    else
    {
        nextT = interpolation->until;
    }

    // Calculate new target value and steps
    double deltaT = (nextT - simTime()).dbl();
    interpolation->currentStepsDone = 0;
    double steps = deltaT / interpolation->interval;

    if (isFirst)
    {
        interpolation->currentValue = interpolation->currentTarget;
        interpolation->currentStep = 0.0;
    }
    else
    {
        interpolation->currentValue = oldTarget;
        interpolation->currentStep = (interpolation->currentTarget - oldTarget) / steps;
    }

    if (!interpolation->currentTargetNode)
        interpolation->currentStep = 0.0;

    if (steps - floor(steps) == 0.0)
    {
        // do not include last update step if it matches a boundary exactly where interpolation change will be called anyway
        steps -= 1.0;
    }
    interpolation->currentStepsTotal = steps > 0.0 ? static_cast<unsigned int> (steps) : 0;

    EV << "New interpolation target: " << interpolation->currentTarget << ", using step " << interpolation->currentStep << " with " << interpolation->currentStepsTotal << " steps" << "\n";

    // send self-msg for next change if there is one
    if (interpolation->currentTargetNode)
    {
        cMessage *msg = new cMessage("interpolation-change-event", SCENARIO_MESSAGE_INTERPOLATION_CHANGE);
        msg->setContextPointer(interpolation);
        scheduleAt(nextT, msg);
    }

    commitInterpolationValue(interpolation);
    // For linear interpolation, this could be skipped for the first time or when currentStep == 0
    if (interpolation->currentStepsTotal > 0)
    {
        scheduleNextInterpolationUpdate(interpolation);
    }
}

void ScenarioManager::commitInterpolationValue(Interpolation *interpolation)
{
    cXMLElement *commands = interpolation->commandsNode;
    for (cXMLElement *node=commands->getFirstChild(); node; node = node->getNextSibling())
    {
        node->setAttribute("value", OPP_Global::dtostr(interpolation->currentValue).c_str());
        processCommand(node);
    }
}

void ScenarioManager::processInterpolationUpdate(Interpolation *interpolation)
{
    interpolation->currentValue += interpolation->currentStep;
    commitInterpolationValue(interpolation);

    interpolation->currentStepsDone++;
    EV << interpolation->currentStepsDone << "/" << interpolation->currentStepsTotal << " interpolation steps done\n";
    if (interpolation->currentStepsDone < interpolation->currentStepsTotal)
    {
        scheduleNextInterpolationUpdate(interpolation);
    }
    else
    {
        EV << "was last interpolation step of this value point, not scheduling update\n";
        // this was the last interpolation point and it is now exhausted -> the interpolation has ended
        if (!interpolation->currentTargetNode)
        {
            EV << "Interpolation has ended\n";
            delete interpolation;
            return;
        }
    }
}

void ScenarioManager::scheduleNextInterpolationUpdate(Interpolation *interpolation)
{
    cMessage *msg = new cMessage("interpolation-update-event", SCENARIO_MESSAGE_INTERPOLATION_UPDATE);
    msg->setContextPointer(interpolation);
    scheduleAt(simTime() + interpolation->interval, msg);
}

void ScenarioManager::processCommand(cXMLElement *node)
{
    const char *tag = node->getTagName();
    EV << "processing <" << tag << "> command...\n";

    if (!strcmp(tag, "at"))
        processAtCommand(node);
    else if (!strcmp(tag, "set-param"))
        processSetParamCommand(node);
    else if (!strcmp(tag, "set-channel-attr"))
        processSetChannelAttrCommand(node);
    // else if (!strcmp(tag,"create-module"))
    //    processCreateModuleCommand(node);
    else if (!strcmp(tag, "connect"))
        processConnectCommand(node);
    else if (!strcmp(tag, "disconnect"))
        processDisconnectCommand(node);
    else
        processModuleSpecificCommand(node);
}

// helper function
static bool parseIndexedName(const char *s, std::string& name, int& index)
{
    const char *b;
    if ((b = strchr(s, '['))==NULL || s[strlen(s)-1]!=']')
    {
        name = s;
        index = -1;
        return false;
    }
    else
    {
        name.assign(s, b-s);
        index = atoi(b+1);
        return true;
    }
}

const char *ScenarioManager::getRequiredAttribute(cXMLElement *node, const char *attr)
{
    const char *s = node->getAttribute(attr);
    if (!s)
        error("required attribute %s of <%s> missing at %s",
              attr, node->getTagName(), node->getSourceLocation());
    return s;
}

cModule *ScenarioManager::getRequiredModule(cXMLElement *node, const char *attr)
{
    const char *moduleAttr = getRequiredAttribute(node, attr);
    cModule *mod = simulation.getModuleByPath(moduleAttr);
    if (!mod)
        error("module '%s' not found at %s", moduleAttr, node->getSourceLocation());
    return mod;
}

cGate *ScenarioManager::getRequiredGate(cXMLElement *node, const char *modAttr, const char *gateAttr)
{
    cModule *mod = getRequiredModule(node, modAttr);
    const char *gateStr = getRequiredAttribute(node, gateAttr);
    std::string gname;
    int gindex;
    parseIndexedName(gateStr, gname, gindex);
    cGate *g = mod->gate(gname.c_str(), gindex);
    if (!g)
        error("module '%s' has no gate '%s' at %s", mod->getFullPath().c_str(), gateStr, node->getSourceLocation());
    return g;
}

void ScenarioManager::processAtCommand(cXMLElement *node)
{
    for (cXMLElement *child=node->getFirstChild(); child; child=child->getNextSibling())
        processCommand(child);
}

void ScenarioManager::processInterpolateCommand(cXMLElement *node)
{
    for (cXMLElement *child=node->getFirstChild(); child; child=child->getNextSibling())
    {
        // check attr t is present
        const char *tAttr = node->getAttribute("t");
        if (!tAttr)
            error("attribute 't' missing at %s", node->getSourceLocation());

        // schedule self-message
        simtime_t t = STR_SIMTIME(tAttr);

        cMessage *msg = new cMessage("scenario-event");
        msg->setContextPointer(node);
        scheduleAt(t, msg);
    }
}

void ScenarioManager::processModuleSpecificCommand(cXMLElement *node)
{
    // find which module we'll need to invoke
    cModule *mod = getRequiredModule(node, "module");

    // see if it supports the IScriptable interface
    IScriptable *scriptable = dynamic_cast<IScriptable *>(mod);
    if (!scriptable)
        error("<%s> not understood: it is not a built-in command of %s, and module class %s "  //TODO be more specific
              "is not scriptable (does not subclass from IScriptable) at %s",
              node->getTagName(), getClassName(), mod->getClassName(), node->getSourceLocation());

    // ok, trust it to process this command
    scriptable->processCommand(*node);
}

void ScenarioManager::processSetParamCommand(cXMLElement *node)
{
    // process <set-param> command
    cModule *mod = getRequiredModule(node, "module");
    const char *parAttr = getRequiredAttribute(node, "par");
    const char *valueAttr = getRequiredAttribute(node, "value");

    EV << "Setting " << mod->getFullPath() << "." << parAttr << " = " << valueAttr << "\n";
    bubble((std::string("setting: ")+mod->getFullPath()+"."+parAttr+" = "+valueAttr).c_str());

    // set the parameter to the given value
    cPar& param = mod->par(parAttr);
    param.parse(valueAttr);
}

void ScenarioManager::processSetChannelAttrCommand(cXMLElement *node)
{
    // process <set-channel-attr> command
    cGate *g = getRequiredGate(node, "src-module", "src-gate");
    const char *attrAttr = getRequiredAttribute(node, "attr");
    const char *valueAttr = getRequiredAttribute(node, "value");

    EV << "Setting channel attribute: " << attrAttr << " = " << valueAttr
       << " of gate " << g->getFullPath() << "\n";
    bubble((std::string("setting channel attr: ")+attrAttr+" = "+valueAttr).c_str());

    // make sure gate is connected at all
    if (!g->getNextGate())
        error("gate '%s' is not connected at %s", g->getFullPath().c_str(), node->getSourceLocation());

    // find channel (or add one?)
    cChannel *chan = g->getChannel();
    if (!chan)
        error("connection starting at gate '%s' has no attributes at %s", g->getFullPath().c_str(), node->getSourceLocation());

    // set the parameter to the given value
    cPar& param = chan->par(attrAttr);
    param.parse(valueAttr);
}

void ScenarioManager::processCreateModuleCommand(cXMLElement *node)
{
    // FIXME finish and test
}

void ScenarioManager::processDeleteModuleCommand(cXMLElement *node)
{
    // FIXME finish and test
}

void ScenarioManager::createConnection(cXMLElementList &paramList, cChannelType *channelType, cGate *srcGate, cGate *destGate)
{
    if (!channelType)
        srcGate->connectTo(destGate);
    else
    {
        cChannel *channel = channelType->create("channel");

        // set parameters:
        for (cXMLElementList::iterator i = paramList.begin(); i != paramList.end(); i++)
        {
            cXMLElement *child = *i;
            const char* name = getRequiredAttribute(child, "name");
            const char* value = getRequiredAttribute(child, "value");
            channel->par(name).parse(value);
        }

        // connect:
        srcGate->connectTo(destGate, channel);
    }
}

void ScenarioManager::processConnectCommand(cXMLElement *node)
{
    cGate *srcGate;
    cModule *srcMod = getRequiredModule(node, "src-module");
    const char *srcGateStr = getRequiredAttribute(node, "src-gate");
    std::string srcGateName;
    int srcGateIndex;
    parseIndexedName(srcGateStr, srcGateName, srcGateIndex);
    bool isSrcGateInOut = (srcMod->gateType(srcGateName.c_str()) == cGate::INOUT);

    cGate *destGate;
    cModule *destMod = getRequiredModule(node, "dest-module");
    const char *destGateStr = getRequiredAttribute(node, "dest-gate");
    std::string destGateName;
    int destGateIndex;
    parseIndexedName(destGateStr, destGateName, destGateIndex);
    bool isDestGateInOut = (destMod->gateType(destGateName.c_str()) == cGate::INOUT);

    if (srcMod->getParentModule() != destMod->getParentModule())
        error("The parent modules of src-module and dest-module are differ at %s",
                node->getSourceLocation());

    // process <connect-channel> command
    const char *channelTypeName = node->getAttribute("channel-type");
    cChannelType *channelType = channelTypeName ? cChannelType::get(channelTypeName) : NULL;
    cXMLElementList paramList;

    if (channelTypeName)
        paramList = node->getChildrenByTagName("param");

    srcGate = isSrcGateInOut ?
            srcMod->gateHalf(srcGateName.c_str(), cGate::OUTPUT, srcGateIndex) :
            srcMod->gate(srcGateName.c_str(), srcGateIndex);
    destGate = isDestGateInOut ?
            destMod->gateHalf(destGateName.c_str(), cGate::INPUT, destGateIndex) :
            destMod->gate(destGateName.c_str(), destGateIndex);

    createConnection(paramList, channelType, srcGate, destGate);

    if (isSrcGateInOut && isDestGateInOut)
    {
        destGate = srcMod->gateHalf(srcGateName.c_str(), cGate::INPUT, srcGateIndex);
        srcGate = destMod->gateHalf(destGateName.c_str(), cGate::OUTPUT, destGateIndex);

        createConnection(paramList, channelType, srcGate, destGate);
    }
}

void ScenarioManager::processDisconnectCommand(cXMLElement *node)
{
    // process <disconnect> command
    cModule *srcMod = getRequiredModule(node, "src-module");
    cModule *parentMod = srcMod->getParentModule();
    const char *srcGateStr = getRequiredAttribute(node, "src-gate");
    std::string srcGateName;
    int srcGateIndex;
    parseIndexedName(srcGateStr, srcGateName, srcGateIndex);
    cGate::Type srcGateType = srcMod->gateType(srcGateName.c_str());

    cGate *srcGate;

    if (srcGateType == cGate::INPUT)
        error("The src-gate must be inout or output gate at %s", node->getSourceLocation());

    if (srcGateType == cGate::INOUT)
    {
        cGate *g;

        srcGate = srcMod->gateHalf(srcGateName.c_str(), cGate::OUTPUT, srcGateIndex);
        g = srcGate->getNextGate();
        if (!g)
            return; // not connected

        if (g->getOwnerModule()->getParentModule() != parentMod)
            error("The src-gate connected to a node on another level at %s", node->getSourceLocation());

        srcGate->disconnect();

        srcGate = srcMod->gateHalf(srcGateName.c_str(), cGate::INPUT, srcGateIndex);
        g = srcGate->getPreviousGate();
        if (!g)
            return; // not connected

        if (g->getOwnerModule()->getParentModule() != parentMod)
            error("The src-gate connected to a node on another level at %s", node->getSourceLocation());

        g->disconnect();
    }
    else
    {
        srcGate = srcMod->gate(srcGateName.c_str(), srcGateIndex);
        cGate *g = srcGate->getNextGate();
        if (g && g->getOwnerModule()->getParentModule() == parentMod)
            srcGate->disconnect();
    }
}

void ScenarioManager::updateDisplayString()
{
    char buf[80];
    sprintf(buf, "total %d changes, %d left", numChanges, numChanges-numDone);
    getDisplayString().setTagArg("t", 0, buf);
}


