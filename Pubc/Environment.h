/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "ToolboxBase/Pubc/Base Environment.h"
#include "HephaestusBase/Pubc/Interface Pipeline.h"

namespace Hephaestus {
namespace Core {

	class Environment : public Toolbox::Base::BaseEnvironment {
        TB_MESSAGES_DECLARE_RECEIVER(Environment, Toolbox::Base::BaseEnvironment);

    protected:
        Core::IPipeline * Pipeline;

    public:
        void InternalSetupRelayMap() override;

        void InternalMessageSendToPip(std::string, Toolbox::Messaging::IAsynchMessage*);
        
    public:
        Environment();
        ~Environment() override;
        
        void UnloadAll() override;
        
        void InternalCompileStats(BlackRoot::Format::JSON &) override;

        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(createPipeline);

        virtual Core::IPipeline * AllocatePipeline();

        void CreatePipeline();
	};

}
}