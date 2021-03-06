#pragma once
#include <vector>
#include <map>
#include <string>
#include "Helper.h"
#include "Constants.h"
#include "TagItem.h"
#include "EuroScopePlugIn.h"

using namespace std;
using namespace EuroScopePlugIn;

class Tag
{
public:

	// General things

	const enum TagStates { NotConcerned, InSequence, Next, TransferredToMe, Assumed, TransferredFromMe, Redundant };

	// TagItems Definition
	TagItem CallsignItem = TagItem::CreatePassive("Callsign", SCREEN_TAG_CALLSIGN, TagItem::TagColourTypes::Highlight);
	TagItem SquawkItem = TagItem::CreatePassive("Squawk");
	TagItem AltitudeItem = TagItem::CreatePassive("Altitude");
	TagItem TendencyItem = TagItem::CreatePassive("Tendency");

	TagItem CoumpoundWarning = TagItem::CreatePassive("Warning", SCREEN_TAG_WARNING, TagItem::TagColourTypes::Information);
	
	// CFL
	TagItem CFLItem = TagItem::CreatePassive("CFL", SCREEN_TAG_CFL);
	// Combination of NFL and XFL, depending on the state
	TagItem XFLItem = TagItem::CreatePassive("XFL", SCREEN_TAG_XFL);
	// RFL
	TagItem RFLItem = TagItem::CreatePassive("RFL", SCREEN_TAG_RFL);
	// COPX/COPN Point Item
	TagItem COPItem = TagItem::CreatePassive("COP", SCREEN_TAG_COP);
	// Lateral clearance (Direct/HDG)
	TagItem HorizontalClearItem = TagItem::CreatePassive("HDG", SCREEN_TAG_HORIZ);
	// Current or next sector
	TagItem SectorItem = TagItem::CreatePassive("Sector", SCREEN_TAG_SECTOR);

	TagItem ReportedGS = TagItem::CreatePassive("ReportedGS");
	TagItem VerticalRate = TagItem::CreatePassive("VerticalRate");

	TagItem AssignedSpeed = TagItem::CreatePassive("AssignedSpeed", SCREEN_TAG_ASPEED);

	TagItem RouteDisplay = TagItem::CreatePassive("R", SCREEN_TAG_ROUTE);
	TagItem SepItem = TagItem::CreatePassive("V", SCREEN_TAG_SEP);

	TagItem BlankItem = TagItem::CreatePassive(" ");

	// Tag definitions
	const vector<vector<TagItem>> MinimizedTag = { { AltitudeItem, TendencyItem } };
	const vector<vector<TagItem>> StandardTag = { { CoumpoundWarning }, { CallsignItem }, 
	{ AltitudeItem, TendencyItem, CFLItem, HorizontalClearItem }, 
	{ XFLItem, COPItem }, { ReportedGS, AssignedSpeed } };
	const vector<vector<TagItem>> MagnifiedTag = { 
	{ CoumpoundWarning },
	{ SepItem, CallsignItem, SectorItem },
	{ RouteDisplay, AltitudeItem, TendencyItem, CFLItem, HorizontalClearItem },
	{ BlankItem, XFLItem, COPItem, RFLItem },
	{ BlankItem, ReportedGS, AssignedSpeed } };

	// Tag Object

	Tag(TagStates State, bool IsMagnified, bool IsSoft, bool isModeAButton, bool isVButton, CRadarScreen* radarScreen, CRadarTarget RadarTarget, CFlightPlan FlightPlan) {

		this->IsMagnified = IsMagnified;
		this->IsSoft = IsSoft;

		map<string, string> TagReplacementMap = GenerateTag(radarScreen, isVButton, isModeAButton, RadarTarget, FlightPlan);

		if (State == TagStates::NotConcerned && IsSoft)
			Definition = MinimizedTag;

		if (State == TagStates::NotConcerned && !IsSoft)
			Definition = StandardTag;

		if (State == TagStates::InSequence && IsSoft)
			Definition = MinimizedTag;

		if (State == TagStates::InSequence && !IsSoft)
			Definition = StandardTag;

		if (State == TagStates::Next || State == TagStates::TransferredToMe 
			|| State == TagStates::Assumed || State == TagStates::TransferredFromMe || State == TagStates::Redundant)
			Definition = StandardTag;

		if (IsMagnified)
			Definition = MagnifiedTag;

		TagState = State;

		// Replacing the tag

		// For each line
		for (auto& TagLine : Definition) {
			// For each item
			for (auto& TagItem : TagLine) {
				// We know replace the item
				for (auto& kv : TagReplacementMap) {
					if (TagItem.Text == kv.first) {
						TagItem.Text = kv.second;
						break;
					}
				}
			}
		}

	};
	~Tag() {};

	vector<vector<TagItem>> Definition;
	TagStates TagState;
	bool IsMagnified;
	bool IsSoft;

protected:
	map<string, string> GenerateTag(CRadarScreen* radarScreen, bool isVButton, bool isModeAButton, CRadarTarget RadarTarget, CFlightPlan FlightPlan) {

		map<string, string> TagReplacementMap;

		if (!RadarTarget.IsValid())
			return TagReplacementMap;

		string callsign = RadarTarget.GetCallsign();

		if (FlightPlan.IsValid() && FlightPlan.GetState() == FLIGHT_PLAN_STATE_TRANSFER_TO_ME_INITIATED)
			callsign = ">>" + callsign;

		if (FlightPlan.IsValid() && FlightPlan.IsTextCommunication())
			callsign += "/t";

		if (FlightPlan.IsValid() && (FlightPlan.GetControllerAssignedData().GetCommunicationType() == 'R'))
			callsign += "/r";

		if (FlightPlan.IsValid() && FlightPlan.GetState() == FLIGHT_PLAN_STATE_TRANSFER_FROM_ME_INITIATED)
			callsign = callsign + ">>";

		TagReplacementMap.insert(pair<string, string>("Callsign", callsign));

		TagReplacementMap.insert(pair<string, string>("Squawk", RadarTarget.GetPosition().GetSquawk()));
		// Alt
		string alt = to_string((int)RoundTo(RadarTarget.GetPosition().GetFlightLevel(), 100) / 100);
		
		if (RadarTarget.GetPosition().GetFlightLevel() <= radarScreen->GetPlugIn()->GetTransitionAltitude())
			alt = "A" + to_string((int)RoundTo(RadarTarget.GetPosition().GetPressureAltitude(), 100) / 100);

		TagReplacementMap.insert(pair<string, string>("Altitude", alt));
		
		string gs = "";
		if (IsMagnified || isVButton) {
			gs = string("G") + to_string(RadarTarget.GetPosition().GetReportedGS());
		}

		TagReplacementMap.insert(pair<string, string>("ReportedGS", gs));

		TagReplacementMap.insert(pair<string, string>("R", " "));
		TagReplacementMap.insert(pair<string, string>("V", " "));

		string VerticalRate = "00";
		CRadarTargetPositionData pos = RadarTarget.GetPosition();
		CRadarTargetPositionData oldpos = RadarTarget.GetPreviousPosition(pos);
		if (pos.IsValid() && oldpos.IsValid()) {
			int deltaalt = pos.GetFlightLevel() - oldpos.GetFlightLevel();
			int deltaT = oldpos.GetReceivedTime() - pos.GetReceivedTime();

			if (deltaT > 0) {
				float vz = abs(deltaalt) * (60.0f / deltaT);

				// If the rate is too much
				if ((int)abs(vz) >= 9999) {
					VerticalRate = "^99";
					if (deltaalt < 0)
						VerticalRate = "|99";
						
				}
				else if (abs(vz) >= 100 && abs(deltaalt) >= 20) {
					string rate = padWithZeros(2, (int)abs(vz / 100));
					VerticalRate = "^" + rate;

					if (deltaalt < 0)
						VerticalRate = "|" + rate;
				}
			}
		}

		TagReplacementMap.insert(pair<string, string>("VerticalRate", VerticalRate));
		
		if (FlightPlan.IsValid()) {
			
			string warning = "";
			if (FlightPlan.GetCLAMFlag())
				warning = "CLAM";
			if (FlightPlan.GetRAMFlag())
				warning = "RAM";
			if (FlightPlan.GetRAMFlag() && FlightPlan.GetCLAMFlag())
				warning = "C+R";

			const char * assr = FlightPlan.GetControllerAssignedData().GetSquawk();
			const char * ssr = RadarTarget.GetPosition().GetSquawk();
			if (isModeAButton) {
				if (warning.length() != 0)
					warning += " ";
				warning += "A" + string(ssr);
			}
			else if (strlen(assr) != 0 && !startsWith(ssr, assr)) {
				if (warning.length() != 0)
					warning += " ";
				warning += string(assr);
			}
			else if (startsWith("2000", ssr) || startsWith("1200", ssr) || startsWith("2200", ssr)) {
				if (warning.length() != 0)
					warning += " ";
				warning += "CODE";
			}

			if (IsMagnified && warning.length() != 0)
				warning = " " + warning;

			TagReplacementMap.insert(pair<string, string>("Warning", warning.c_str()));

			// RFL
			string RFL = padWithZeros(3, FlightPlan.GetControllerAssignedData().GetFinalAltitude() / 100);

			if (FlightPlan.GetControllerAssignedData().GetFinalAltitude() == 0)
				RFL = padWithZeros(3, FlightPlan.GetFlightPlanData().GetFinalAltitude() / 100);

			RFL = RFL.substr(0, 2);

			TagReplacementMap.insert(pair<string, string>("RFL", RFL));

			// CFL
			string CFL = padWithZeros(3, FlightPlan.GetClearedAltitude() / 100);
			CFL = CFL.substr(0, 2);

			if (FlightPlan.GetClearedAltitude() == 0)
				CFL = RFL;

			// If not detailed and reached alt, then nothing to show
			if (!IsMagnified && abs(RadarTarget.GetPosition().GetFlightLevel()- FlightPlan.GetClearedAltitude()) < 100)
				CFL = "";

			if (FlightPlan.GetControllerAssignedData().GetClearedAltitude() == 1)
				CFL = "�";

			if (FlightPlan.GetControllerAssignedData().GetClearedAltitude() == 2)
				CFL = "�";

			TagReplacementMap.insert(pair<string, string>("CFL", CFL));
			
			string assignedSpeed = "";

			// If there is a speed
			if (FlightPlan.GetControllerAssignedData().GetAssignedSpeed() != 0 ||
				FlightPlan.GetControllerAssignedData().GetAssignedMach() != 0) {
				if (!IsMagnified) {
					assignedSpeed = "S";
				}
				else {
					if (FlightPlan.GetControllerAssignedData().GetAssignedMach() != 0) {
						assignedSpeed = "M" + to_string(FlightPlan.GetControllerAssignedData().GetAssignedMach() / 100.0).substr(0, 4);
					}
					else {
						assignedSpeed = "S" + to_string(FlightPlan.GetControllerAssignedData().GetAssignedSpeed());
					}
				}
			}
			else if (IsMagnified) {
				assignedSpeed = "   ";
			}
			

			TagReplacementMap.insert(pair<string, string>("AssignedSpeed", assignedSpeed));

			// Sector
			string SectorId = "   ";
			if (FlightPlan.GetTrackingControllerIsMe()) {
				string controllerCallsign = FlightPlan.GetCoordinatedNextController();
				if (controllerCallsign.length()  > 0) {
					SectorId = radarScreen->GetPlugIn()->ControllerSelect(controllerCallsign.c_str()).GetPositionId();
				}
			}
			else {
				string controllerCallsign = FlightPlan.GetTrackingControllerId();
				if (controllerCallsign.length() > 0) {
					SectorId = controllerCallsign;
				}
			}

			TagReplacementMap.insert(pair<string, string>("Sector", SectorId));

			string Horizontal = "  ";
			if (IsMagnified) {
				if (FlightPlan.GetControllerAssignedData().GetAssignedHeading() != 0) {
					Horizontal = "H" + padWithZeros(3, FlightPlan.GetControllerAssignedData().GetAssignedHeading());
				}
				else if (strlen(FlightPlan.GetControllerAssignedData().GetDirectToPointName()) != 0) {
					Horizontal = FlightPlan.GetControllerAssignedData().GetDirectToPointName();
				}
			}
			else {
				if (FlightPlan.GetControllerAssignedData().GetAssignedHeading() != 0)
					Horizontal = "H";
				else
					Horizontal = "";
			}

			TagReplacementMap.insert(pair<string, string>("HDG", Horizontal));

			string XFL = "";
			if (IsMagnified)
				XFL = "  ";

			// if assumed, then COPX alt is shown, else COPN alt
			if (FlightPlan.GetTrackingControllerIsMe()) {
				if (FlightPlan.GetExitCoordinationAltitude() != 0) {
					XFL = padWithZeros(3, FlightPlan.GetExitCoordinationAltitude() / 100);
					XFL = XFL.substr(0, 2);

					if (FlightPlan.GetExitCoordinationAltitudeState() == COORDINATION_STATE_REQUESTED_BY_ME ||
						FlightPlan.GetExitCoordinationAltitudeState() == COORDINATION_STATE_REQUESTED_BY_OTHER)
						XFL = PREFIX_PURPLE_COLOR + XFL;
				}
					
			}
			else {
				if (FlightPlan.GetEntryCoordinationAltitude() != 0) {
					XFL = padWithZeros(3, FlightPlan.GetEntryCoordinationAltitude() / 100);
					XFL = XFL.substr(0, 2);

					if (FlightPlan.GetEntryCoordinationAltitudeState() == COORDINATION_STATE_REQUESTED_BY_ME ||
						FlightPlan.GetEntryCoordinationAltitudeState() == COORDINATION_STATE_REQUESTED_BY_OTHER)
						XFL = PREFIX_PURPLE_COLOR + XFL;
				}
					
			}

			if (!IsMagnified && XFL.length() != 0 && XFL == CFL)
				XFL = "";

			TagReplacementMap.insert(pair<string, string>("XFL", XFL));
			
			string COP = "";
			if (IsMagnified)
				COP = "COPX";

			// if assumed, then COPX  is shown, else COPN
			if (FlightPlan.GetTrackingControllerIsMe()) {
				if (strlen(FlightPlan.GetExitCoordinationPointName()) != 0) {
					COP = FlightPlan.GetExitCoordinationPointName();

					if (FlightPlan.GetExitCoordinationNameState() == COORDINATION_STATE_REQUESTED_BY_ME ||
						FlightPlan.GetExitCoordinationNameState() == COORDINATION_STATE_REQUESTED_BY_OTHER)
						COP = PREFIX_PURPLE_COLOR + COP;
				}
			}
			else {
				if (strlen(FlightPlan.GetEntryCoordinationPointName()) != 0) {
					COP = FlightPlan.GetEntryCoordinationPointName();

					if (FlightPlan.GetEntryCoordinationPointState() == COORDINATION_STATE_REQUESTED_BY_ME ||
						FlightPlan.GetEntryCoordinationPointState() == COORDINATION_STATE_REQUESTED_BY_OTHER)
						COP = PREFIX_PURPLE_COLOR + COP;
				}
			}

			if (!IsMagnified && COP.length() != 0 && COP == string(FlightPlan.GetControllerAssignedData().GetDirectToPointName()))
				COP = "";

			TagReplacementMap.insert(pair<string, string>("COP", COP));
		}

		string tendency = "-";
		int delta_fl = RadarTarget.GetPosition().GetFlightLevel() -
			RadarTarget.GetPreviousPosition(RadarTarget.GetPosition()).GetFlightLevel();
		if (abs(delta_fl) >= 50)
			tendency = delta_fl < 0 ? "|" : "^";

		TagReplacementMap.insert(pair<string, string>("Tendency", tendency));

		return TagReplacementMap;
	}
};

