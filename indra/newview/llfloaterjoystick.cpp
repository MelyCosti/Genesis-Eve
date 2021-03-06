/** 
 * @file llfloaterjoystick.cpp
 * @brief Joystick preferences panel
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

// file include
#include "llfloaterjoystick.h"

// linden library includes
#include "llerror.h"
#include "llrect.h"
#include "llstring.h"

// project includes
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llappviewer.h"
#include "llviewerjoystick.h"
#include "llcheckboxctrl.h"

LLFloaterJoystick::LLFloaterJoystick(const LLSD& data)
	: LLFloater("floater_joystick")
	, mCheckJoystickEnabled(nullptr)
	, mCheckFlycamEnabled(nullptr)
	, mAxisStatsBar({ {nullptr,nullptr,nullptr,nullptr,nullptr,nullptr} })
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_joystick.xml");
	center();
}

void LLFloaterJoystick::draw()
{
	// Singu TODO: Cache these children, or consider not doing this in the draw call
	bool joystick_inited = LLViewerJoystick::getInstance()->isJoystickInitialized();
	childSetEnabled("enable_joystick", joystick_inited);
	auto type(getChild<LLView>("joystick_type"));
	type->setEnabled(joystick_inited);
	std::string desc = LLViewerJoystick::getInstance()->getDescription();
	if (desc.empty()) desc = getString("NoDevice");
	type->setValue(desc);

	LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
	for (U32 i = 0; i < 6; ++i)
	{
		F32 value = joystick->getJoystickAxis(i);
		mAxisStatsBar[i]->fit(value);
		mAxisStats[i]->addValue(value * gFrameIntervalSeconds);
	}

	LLFloater::draw();
}

BOOL LLFloaterJoystick::postBuild()
{		
	F32 range = gSavedSettings.getBOOL("Cursor3D") ? 1024.f : 2.f;
	LLUIString axis = getString("Axis");
	LLUIString joystick = getString("JoystickMonitor");

	// use this child to get relative positioning info; we'll place the
	// joystick monitor on its right, vertically aligned to it.
	LLView* child = getChild<LLView>("FlycamAxisScale1");
	LLRect rect;

	if (child)
	{
		LLRect r = child->getRect();
		rect = LLRect(350, r.mTop, r.mRight + 200, 0);
	}


	LLStatView::Params params;
	params.name("axis values");
	params.rect(rect);
	params.show_label(true);
	params.label(joystick);
	mAxisStatsView = LLUICtrlFactory::create<LLStatView>(params);

	for (U32 i = 0; i < 6; i++)
	{
		axis.setArg("[NUM]", llformat("%d", i));
		std::string stat_name(llformat("Joystick axis %d", i));
		mAxisStats[i] = new LLStat(stat_name,4);
		LLStatBar::Parameters params;
		params.mMinBar = -range;
		params.mMaxBar = range;
		params.mLabelSpacing = range * 0.5f;
		params.mTickSpacing = range * 0.25f;
		mAxisStatsBar[i] = mAxisStatsView->addStat(axis, mAxisStats[i], params);
	}

	addChild(mAxisStatsView);
	
	mCheckJoystickEnabled = getChild<LLCheckBoxCtrl>("enable_joystick");
	childSetCommitCallback("enable_joystick",onCommitJoystickEnabled,this);
	mCheckFlycamEnabled = getChild<LLCheckBoxCtrl>("JoystickFlycamEnabled");
	childSetCommitCallback("JoystickFlycamEnabled",onCommitJoystickEnabled,this);

	getChild<LLUICtrl>("Default")->setCommitCallback(boost::bind(&LLFloaterJoystick::onClickDefault, this, _2));
	childSetAction("cancel_btn", onClickCancel, this);
	childSetAction("ok_btn", onClickOK, this);

	refresh();
	return TRUE;
}

LLFloaterJoystick::~LLFloaterJoystick()
{
	// Children all cleaned up by default view destructor.
}


void LLFloaterJoystick::apply()
{
}

void LLFloaterJoystick::refresh()
{
	LLFloater::refresh();

	mJoystickEnabled = gSavedSettings.getBOOL("JoystickEnabled");

	mJoystickAxis[0] = gSavedSettings.getS32("JoystickAxis0");
	mJoystickAxis[1] = gSavedSettings.getS32("JoystickAxis1");
	mJoystickAxis[2] = gSavedSettings.getS32("JoystickAxis2");
	mJoystickAxis[3] = gSavedSettings.getS32("JoystickAxis3");
	mJoystickAxis[4] = gSavedSettings.getS32("JoystickAxis4");
	mJoystickAxis[5] = gSavedSettings.getS32("JoystickAxis5");
	mJoystickAxis[6] = gSavedSettings.getS32("JoystickAxis6");
	
	m3DCursor = gSavedSettings.getBOOL("Cursor3D");
	mAutoLeveling = gSavedSettings.getBOOL("AutoLeveling");
	mZoomDirect  = gSavedSettings.getBOOL("ZoomDirect");

	mAvatarEnabled = gSavedSettings.getBOOL("JoystickAvatarEnabled");
	mBuildEnabled = gSavedSettings.getBOOL("JoystickBuildEnabled");
	mFlycamEnabled = gSavedSettings.getBOOL("JoystickFlycamEnabled");
	
	mAvatarAxisScale[0] = gSavedSettings.getF32("AvatarAxisScale0");
	mAvatarAxisScale[1] = gSavedSettings.getF32("AvatarAxisScale1");
	mAvatarAxisScale[2] = gSavedSettings.getF32("AvatarAxisScale2");
	mAvatarAxisScale[3] = gSavedSettings.getF32("AvatarAxisScale3");
	mAvatarAxisScale[4] = gSavedSettings.getF32("AvatarAxisScale4");
	mAvatarAxisScale[5] = gSavedSettings.getF32("AvatarAxisScale5");

	mBuildAxisScale[0] = gSavedSettings.getF32("BuildAxisScale0");
	mBuildAxisScale[1] = gSavedSettings.getF32("BuildAxisScale1");
	mBuildAxisScale[2] = gSavedSettings.getF32("BuildAxisScale2");
	mBuildAxisScale[3] = gSavedSettings.getF32("BuildAxisScale3");
	mBuildAxisScale[4] = gSavedSettings.getF32("BuildAxisScale4");
	mBuildAxisScale[5] = gSavedSettings.getF32("BuildAxisScale5");

	mFlycamAxisScale[0] = gSavedSettings.getF32("FlycamAxisScale0");
	mFlycamAxisScale[1] = gSavedSettings.getF32("FlycamAxisScale1");
	mFlycamAxisScale[2] = gSavedSettings.getF32("FlycamAxisScale2");
	mFlycamAxisScale[3] = gSavedSettings.getF32("FlycamAxisScale3");
	mFlycamAxisScale[4] = gSavedSettings.getF32("FlycamAxisScale4");
	mFlycamAxisScale[5] = gSavedSettings.getF32("FlycamAxisScale5");
	mFlycamAxisScale[6] = gSavedSettings.getF32("FlycamAxisScale6");

	mAvatarAxisDeadZone[0] = gSavedSettings.getF32("AvatarAxisDeadZone0");
	mAvatarAxisDeadZone[1] = gSavedSettings.getF32("AvatarAxisDeadZone1");
	mAvatarAxisDeadZone[2] = gSavedSettings.getF32("AvatarAxisDeadZone2");
	mAvatarAxisDeadZone[3] = gSavedSettings.getF32("AvatarAxisDeadZone3");
	mAvatarAxisDeadZone[4] = gSavedSettings.getF32("AvatarAxisDeadZone4");
	mAvatarAxisDeadZone[5] = gSavedSettings.getF32("AvatarAxisDeadZone5");

	mBuildAxisDeadZone[0] = gSavedSettings.getF32("BuildAxisDeadZone0");
	mBuildAxisDeadZone[1] = gSavedSettings.getF32("BuildAxisDeadZone1");
	mBuildAxisDeadZone[2] = gSavedSettings.getF32("BuildAxisDeadZone2");
	mBuildAxisDeadZone[3] = gSavedSettings.getF32("BuildAxisDeadZone3");
	mBuildAxisDeadZone[4] = gSavedSettings.getF32("BuildAxisDeadZone4");
	mBuildAxisDeadZone[5] = gSavedSettings.getF32("BuildAxisDeadZone5");

	mFlycamAxisDeadZone[0] = gSavedSettings.getF32("FlycamAxisDeadZone0");
	mFlycamAxisDeadZone[1] = gSavedSettings.getF32("FlycamAxisDeadZone1");
	mFlycamAxisDeadZone[2] = gSavedSettings.getF32("FlycamAxisDeadZone2");
	mFlycamAxisDeadZone[3] = gSavedSettings.getF32("FlycamAxisDeadZone3");
	mFlycamAxisDeadZone[4] = gSavedSettings.getF32("FlycamAxisDeadZone4");
	mFlycamAxisDeadZone[5] = gSavedSettings.getF32("FlycamAxisDeadZone5");
	mFlycamAxisDeadZone[6] = gSavedSettings.getF32("FlycamAxisDeadZone6");

	mAvatarFeathering = gSavedSettings.getF32("AvatarFeathering");
	mBuildFeathering = gSavedSettings.getF32("BuildFeathering");
	mFlycamFeathering = gSavedSettings.getF32("FlycamFeathering");
}

void LLFloaterJoystick::cancel()
{
	gSavedSettings.setBOOL("JoystickEnabled", mJoystickEnabled);

	gSavedSettings.setS32("JoystickAxis0", mJoystickAxis[0]);
	gSavedSettings.setS32("JoystickAxis1", mJoystickAxis[1]);
	gSavedSettings.setS32("JoystickAxis2", mJoystickAxis[2]);
	gSavedSettings.setS32("JoystickAxis3", mJoystickAxis[3]);
	gSavedSettings.setS32("JoystickAxis4", mJoystickAxis[4]);
	gSavedSettings.setS32("JoystickAxis5", mJoystickAxis[5]);
	gSavedSettings.setS32("JoystickAxis6", mJoystickAxis[6]);

	gSavedSettings.setBOOL("Cursor3D", m3DCursor);
	gSavedSettings.setBOOL("AutoLeveling", mAutoLeveling);
	gSavedSettings.setBOOL("ZoomDirect", mZoomDirect );

	gSavedSettings.setBOOL("JoystickAvatarEnabled", mAvatarEnabled);
	gSavedSettings.setBOOL("JoystickBuildEnabled", mBuildEnabled);
	gSavedSettings.setBOOL("JoystickFlycamEnabled", mFlycamEnabled);
	
	gSavedSettings.setF32("AvatarAxisScale0", mAvatarAxisScale[0]);
	gSavedSettings.setF32("AvatarAxisScale1", mAvatarAxisScale[1]);
	gSavedSettings.setF32("AvatarAxisScale2", mAvatarAxisScale[2]);
	gSavedSettings.setF32("AvatarAxisScale3", mAvatarAxisScale[3]);
	gSavedSettings.setF32("AvatarAxisScale4", mAvatarAxisScale[4]);
	gSavedSettings.setF32("AvatarAxisScale5", mAvatarAxisScale[5]);

	gSavedSettings.setF32("BuildAxisScale0", mBuildAxisScale[0]);
	gSavedSettings.setF32("BuildAxisScale1", mBuildAxisScale[1]);
	gSavedSettings.setF32("BuildAxisScale2", mBuildAxisScale[2]);
	gSavedSettings.setF32("BuildAxisScale3", mBuildAxisScale[3]);
	gSavedSettings.setF32("BuildAxisScale4", mBuildAxisScale[4]);
	gSavedSettings.setF32("BuildAxisScale5", mBuildAxisScale[5]);

	gSavedSettings.setF32("FlycamAxisScale0", mFlycamAxisScale[0]);
	gSavedSettings.setF32("FlycamAxisScale1", mFlycamAxisScale[1]);
	gSavedSettings.setF32("FlycamAxisScale2", mFlycamAxisScale[2]);
	gSavedSettings.setF32("FlycamAxisScale3", mFlycamAxisScale[3]);
	gSavedSettings.setF32("FlycamAxisScale4", mFlycamAxisScale[4]);
	gSavedSettings.setF32("FlycamAxisScale5", mFlycamAxisScale[5]);
	gSavedSettings.setF32("FlycamAxisScale6", mFlycamAxisScale[6]);

	gSavedSettings.setF32("AvatarAxisDeadZone0", mAvatarAxisDeadZone[0]);
	gSavedSettings.setF32("AvatarAxisDeadZone1", mAvatarAxisDeadZone[1]);
	gSavedSettings.setF32("AvatarAxisDeadZone2", mAvatarAxisDeadZone[2]);
	gSavedSettings.setF32("AvatarAxisDeadZone3", mAvatarAxisDeadZone[3]);
	gSavedSettings.setF32("AvatarAxisDeadZone4", mAvatarAxisDeadZone[4]);
	gSavedSettings.setF32("AvatarAxisDeadZone5", mAvatarAxisDeadZone[5]);

	gSavedSettings.setF32("BuildAxisDeadZone0", mBuildAxisDeadZone[0]);
	gSavedSettings.setF32("BuildAxisDeadZone1", mBuildAxisDeadZone[1]);
	gSavedSettings.setF32("BuildAxisDeadZone2", mBuildAxisDeadZone[2]);
	gSavedSettings.setF32("BuildAxisDeadZone3", mBuildAxisDeadZone[3]);
	gSavedSettings.setF32("BuildAxisDeadZone4", mBuildAxisDeadZone[4]);
	gSavedSettings.setF32("BuildAxisDeadZone5", mBuildAxisDeadZone[5]);

	gSavedSettings.setF32("FlycamAxisDeadZone0", mFlycamAxisDeadZone[0]);
	gSavedSettings.setF32("FlycamAxisDeadZone1", mFlycamAxisDeadZone[1]);
	gSavedSettings.setF32("FlycamAxisDeadZone2", mFlycamAxisDeadZone[2]);
	gSavedSettings.setF32("FlycamAxisDeadZone3", mFlycamAxisDeadZone[3]);
	gSavedSettings.setF32("FlycamAxisDeadZone4", mFlycamAxisDeadZone[4]);
	gSavedSettings.setF32("FlycamAxisDeadZone5", mFlycamAxisDeadZone[5]);
	gSavedSettings.setF32("FlycamAxisDeadZone6", mFlycamAxisDeadZone[6]);

	gSavedSettings.setF32("AvatarFeathering", mAvatarFeathering);
	gSavedSettings.setF32("BuildFeathering", mBuildFeathering);
	gSavedSettings.setF32("FlycamFeathering", mFlycamFeathering);
}

void LLFloaterJoystick::onCommitJoystickEnabled(LLUICtrl*, void *joy_panel)
{
	LLFloaterJoystick* self = (LLFloaterJoystick*)joy_panel;
	BOOL joystick_enabled = self->mCheckJoystickEnabled->get();
	BOOL flycam_enabled = self->mCheckFlycamEnabled->get();

	if (!joystick_enabled || !flycam_enabled)
	{
		// Turn off flycam
		LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
		if (joystick->getOverrideCamera())
		{
			joystick->toggleFlycam();
		}
	}
}

S32 get_joystick_type();
void LLFloaterJoystick::onClickDefault(const LLSD& val)
{
	S32 type(val.asInteger());
	if (val.isUndefined()) // If button portion, set to default for device.
		if ((type = get_joystick_type()) == -1) // Invalid/No device
			return;
	LLViewerJoystick::getInstance()->setSNDefaults(type);
}

void LLFloaterJoystick::onClickCancel(void *joy_panel)
{
	if (joy_panel)
	{
		LLFloaterJoystick* self = (LLFloaterJoystick*)joy_panel;

		if (self)
		{
			self->cancel();
			self->close();
		}
	}
}

void LLFloaterJoystick::onClickOK(void *joy_panel)
{
	if (joy_panel)
	{
		LLFloaterJoystick* self = (LLFloaterJoystick*)joy_panel;

		if (self)
		{
			self->close();
		}
	}
}
