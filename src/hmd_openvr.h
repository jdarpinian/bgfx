/*
 * Copyright 2011-2018 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#ifndef BGFX_OPENVR_H_HEADER_GUARD
#define BGFX_OPENVR_H_HEADER_GUARD

#include "bgfx_p.h"

#if BGFX_CONFIG_USE_OPENVR

#include "hmd.h"
#include <openvr.h>

namespace bgfx
{
	class VRImplOpenVR : public VRImplI
	{
	public:
		VRImplOpenVR();
		~VRImplOpenVR();

		virtual bool init() override;
		virtual void shutdown() override;
		virtual void connect(VRDesc* _desc) override;
		virtual void disconnect() override;
		virtual bool isConnected() const override
		{
			return NULL != m_compositor;
		}

		virtual bool updateTracking(HMD& _hmd) override;
		virtual void updateInput(HMD& _hmd) override;
		virtual void recenter() override;

		virtual bool createSwapChain(const VRDesc& _desc, int _msaaSamples, int _mirrorWidth, int _mirrorHeight) override = 0;
		virtual void destroySwapChain() override = 0;
		virtual void destroyMirror() override = 0;
		virtual void makeRenderTargetActive(const VRDesc& _desc) override = 0;
		virtual bool submitSwapChain(const VRDesc& _desc) override = 0;

	protected:
		struct EyeOffset
		{
			float offset[3];
		};

		vr::IVRCompositor* m_compositor;
		vr::IVRSystem* m_system;
		EyeOffset m_eyeOffsets[2];
		vr::TrackedDeviceIndex_t m_leftControllerId = vr::k_unTrackedDeviceIndexInvalid;
		vr::TrackedDeviceIndex_t m_rightControllerId = vr::k_unTrackedDeviceIndexInvalid;
	};

} // namespace bgfx

#endif // BGFX_CONFIG_USE_OPENVR
#endif // BGFX_OPENVR_H_HEADER_GUARD
