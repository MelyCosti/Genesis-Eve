/** 
 * @file llfloatermute.h
 * @brief Container for mute list
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERMUTE_H
#define LL_LLFLOATERMUTE_H

#include "llfloater.h"
#include "llmutelist.h"
#include <vector>

class LLAvatarName;
class LLButton;
class LLLineEditor;
class LLMessageSystem;
class LLUUID;
class LLNameListCtrl;
class LLMute;

class LLFloaterMute
	:	public LLFloater, public LLMuteListObserver, public LLFloaterSingleton<LLFloaterMute>
{
public:
	LLFloaterMute(const LLSD& seed);
	~LLFloaterMute();

	// Must have one global floater so chat history can
	// be kept in the text editor.
	virtual void onClose(bool app_quitting) { setVisible(FALSE); }
	virtual BOOL postBuild();

	void refreshMuteList();
	void selectMute(const LLUUID& id);

	void updateButtons();

	// LLMuteListObserver callback interface implementation.
	/* virtual */ void onChange() { refreshMuteList(); }

private:
	// UI callbacks
	static void onClickRemove(void *data);
	static void onClickPick(void *data);
	void onPickUser(const uuid_vec_t& ids, const std::vector<LLAvatarName>& names);
	static void onClickMuteByName(void*);
	static void callbackMuteByName(const std::string& text, void*);
	void showProfile() const;

private:
	LLNameListCtrl*			mMuteList;

	LLPointer<LLUIImage>	mAvatarIcon;	//icon_avatar_offline.tga
	LLPointer<LLUIImage>	mObjectIcon;	//inv_item_object.tga
	LLPointer<LLUIImage>	mGroupIcon;		//icon_group.tga
	LLPointer<LLUIImage>	mNameIcon;		//icon_name.tga

	std::map<LLUUID, LLMute>	mMuteDict;	//Easiest way to associate listitems with LLMute instances without hacking in, say, a hidden column.
};


#endif
