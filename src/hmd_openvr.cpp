/*
 * Copyright 2011-2018 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "hmd_openvr.h"

#if BGFX_CONFIG_USE_OPENVR

#include <memory>

namespace bgfx
{
	static void openvrTransformToQuat(float* quat, const float mat34[3][4])
	{
		const float trace = mat34[0][0] + mat34[1][1] + mat34[2][2];

		if (trace > 0.0f)
		{
			const float s = 2.0f * sqrtf(1.0f + trace);
			quat[0] = (mat34[2][1] - mat34[1][2]) / s;
			quat[1] = (mat34[0][2] - mat34[2][0]) / s;
			quat[2] = (mat34[1][0] - mat34[0][1]) / s;
			quat[3] = s * 0.25f;
		}
		else if ((mat34[0][0] > mat34[1][1]) && (mat34[0][0] > mat34[2][2]))
		{
			const float s = 2.0f * sqrtf(1.0f + mat34[0][0] - mat34[1][1] - mat34[2][2]);
			quat[0] = s * 0.25f;
			quat[1] = (mat34[0][1] + mat34[1][0]) / s;
			quat[2] = (mat34[2][0] + mat34[0][2]) / s;
			quat[3] = (mat34[2][1] - mat34[1][2]) / s;
		}
		else if (mat34[1][1] > mat34[2][2])
		{
			const float s = 2.0f * sqrtf(1.0f + mat34[1][1] - mat34[0][0] - mat34[2][2]);
			quat[0] = (mat34[0][1] + mat34[1][0]) / s;
			quat[1] = s * 0.25f;
			quat[2] = (mat34[1][2] + mat34[2][1]) / s;
			quat[3] = (mat34[0][2] - mat34[2][0]) / s;
		}
		else
		{
			const float s = 2.0f * sqrtf(1.0f + mat34[2][2] - mat34[0][0] - mat34[1][1]);
			quat[0] = (mat34[0][2] + mat34[2][0]) / s;
			quat[1] = (mat34[1][2] + mat34[2][1]) / s;
			quat[2] = s * 0.25f;
			quat[3] = (mat34[1][0] - mat34[0][1]) / s;
		}
	}

	VRImplOpenVR::VRImplOpenVR()
		: m_system(NULL), m_compositor(NULL)
	{
	}

	VRImplOpenVR::~VRImplOpenVR()
	{
		if (NULL != g_platformData.session)
		{
			return;
		}

		BX_CHECK(NULL == m_system, "OpenVR not shutdown properly.");
	}

	bool VRImplOpenVR::init()
	{
		return vr::VR_IsRuntimeInstalled();
	}

	void VRImplOpenVR::shutdown()
	{
		if (NULL != g_platformData.session)
		{
			return;
		}

		vr::VR_Shutdown();
		m_system = NULL;
		m_compositor = NULL;
		g_internalData.session = NULL;
		g_internalData.compositor = NULL;
	}

	void VRImplOpenVR::connect(VRDesc* desc)
	{
		vr::EVRInitError err;

		if (NULL == g_platformData.session)
		{
			m_system = vr::VR_Init(&err, vr::VRApplication_Scene);
			if (err != vr::VRInitError_None)
			{
				shutdown();
				BX_TRACE("Failed to initialize OpenVR: %d", err);
				return;
			}
			g_internalData.session = (vr::IVRSystem*)m_system;
		}
		else
		{
			m_system = (vr::IVRSystem*)g_platformData.session;
		}

		// get the compositor
		if (NULL == g_platformData.compositor)
		{
			m_compositor = static_cast<vr::IVRCompositor*>(vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &err));
			if (!m_compositor)
			{
				shutdown();
				BX_TRACE("Failed to obtain compositor interface: %d", err);
				return;
			}
			g_internalData.compositor = (vr::IVRCompositor*)m_compositor;
		}
		else
		{
			m_compositor = (vr::IVRCompositor*)g_platformData.compositor;
		}

		vr::IVRExtendedDisplay* extdisp = static_cast<vr::IVRExtendedDisplay*>(vr::VR_GetGenericInterface(vr::IVRExtendedDisplay_Version, &err));
		if (!extdisp)
		{
			shutdown();
			BX_TRACE("Failed to obtain extended display interface: %d", err);
			return;
		}

		int32_t x, y;
		extdisp->GetWindowBounds(&x, &y, &desc->m_deviceSize.m_w, &desc->m_deviceSize.m_h);

		m_system->GetRecommendedRenderTargetSize(&desc->m_eyeSize[0].m_w, &desc->m_eyeSize[0].m_h);
		desc->m_eyeSize[1] = desc->m_eyeSize[0];

		desc->m_deviceType = 1; // TODO: Set this to something reasonable
		desc->m_refreshRate = m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);

		vr::ETrackedPropertyError propErr;
		desc->m_neckOffset[0] = m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_UserHeadToEyeDepthMeters_Float, &propErr);
		if (desc->m_neckOffset[0] == 0.0f)
		{
			desc->m_neckOffset[0] = 0.0805f;
		}
		desc->m_neckOffset[1] = 0.075f;

		for (int eye = 0; eye != 2; ++eye)
		{
			m_system->GetProjectionRaw(static_cast<vr::EVREye>(eye), &desc->m_eyeFov[eye].m_left, &desc->m_eyeFov[eye].m_right, &desc->m_eyeFov[eye].m_down, &desc->m_eyeFov[eye].m_up);
			desc->m_eyeFov[eye].m_left *= -1.0f;
			desc->m_eyeFov[eye].m_down *= -1.0f;

			auto xform = m_system->GetEyeToHeadTransform(static_cast<vr::EVREye>(eye));
			m_eyeOffsets[eye].offset[0] = xform.m[0][3];
			m_eyeOffsets[eye].offset[1] = xform.m[1][3];
			m_eyeOffsets[eye].offset[2] = xform.m[2][3];
		}
	}

	void VRImplOpenVR::disconnect()
	{
		shutdown();
		m_leftControllerId = m_rightControllerId = vr::k_unTrackedDeviceIndexInvalid;
	}

	bool VRImplOpenVR::updateTracking(HMD& _hmd)
	{
		if (NULL == m_compositor)
		{
			return false;
		}

		vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
		auto err = m_compositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0);
		if (err != vr::VRCompositorError_None)
		{
			return false;
		}

		const auto& headPose = poses[vr::k_unTrackedDeviceIndex_Hmd];
		if (headPose.bPoseIsValid)
		{
			if (headPose.eTrackingResult == vr::TrackingResult_Running_OK)
			{
				// convert to position/quat
				float head_position[3];
				float head_rotation[4];
				head_position[0] = headPose.mDeviceToAbsoluteTracking.m[0][3];
				head_position[1] = headPose.mDeviceToAbsoluteTracking.m[1][3];
				head_position[2] = headPose.mDeviceToAbsoluteTracking.m[2][3];
				openvrTransformToQuat(head_rotation, headPose.mDeviceToAbsoluteTracking.m);

				// calculate eye translations in tracked space
				for (int eye = 0; eye != 2; ++eye)
				{
					const float uvx = 2.0f * (head_rotation[1] * m_eyeOffsets[eye].offset[2] - head_rotation[2] * m_eyeOffsets[eye].offset[1]);
					const float uvy = 2.0f * (head_rotation[2] * m_eyeOffsets[eye].offset[0] - head_rotation[0] * m_eyeOffsets[eye].offset[2]);
					const float uvz = 2.0f * (head_rotation[0] * m_eyeOffsets[eye].offset[1] - head_rotation[1] * m_eyeOffsets[eye].offset[0]);
					_hmd.eye[eye].translation[0] = m_eyeOffsets[eye].offset[0] + head_rotation[3] * uvx + head_rotation[1] * uvz - head_rotation[2] * uvy + head_position[0];
					_hmd.eye[eye].translation[1] = m_eyeOffsets[eye].offset[1] + head_rotation[3] * uvy + head_rotation[2] * uvx - head_rotation[0] * uvz + head_position[1];
					_hmd.eye[eye].translation[2] = m_eyeOffsets[eye].offset[2] + head_rotation[3] * uvz + head_rotation[0] * uvy - head_rotation[1] * uvx + head_position[2];
				}
			}
		}

		return true;
	}

	void VRImplOpenVR::updateInput(HMD& _hmd)
	{
	}

	void VRImplOpenVR::recenter()
	{
		if (NULL != m_system)
		{
			m_system->ResetSeatedZeroPose();
		}
	}
}

#endif // BGFX_CONFIG_USE_OPENVR
