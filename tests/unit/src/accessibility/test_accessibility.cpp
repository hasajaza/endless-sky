/* test_accessibility.cpp
Copyright (c) 2026 by the Endless Sky Accessibility contributors

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "es-test.hpp"

#include "../../../../source/accessibility/AccessCapture.h"
#include "../../../../source/accessibility/AccessFlight.h"
#include "../../../../source/accessibility/AccessFormat.h"
#include "../../../../source/accessibility/AccessReaders.h"
#include "../../../../source/accessibility/AccessSpeech.h"
#include "../../../../source/Angle.h"
#include "../../../../source/Color.h"
#include "../../../../source/Point.h"

#include <string>
#include <vector>

namespace { // test namespace

// #region unit tests
TEST_CASE( "Relative bearings follow the game's angle convention", "[accessibility][format]" ) {
	// The game's angle 0 points up the screen and positive angles turn clockwise.
	Angle facingUp(0.);
	CHECK_THAT( AccessFormat::RelativeDegrees(facingUp, Point(0., -100.)), Catch::Matchers::WithinAbs(0., 0.001) );
	CHECK_THAT( AccessFormat::RelativeDegrees(facingUp, Point(100., 0.)), Catch::Matchers::WithinAbs(90., 0.001) );
	CHECK_THAT( AccessFormat::RelativeDegrees(facingUp, Point(-100., 0.)), Catch::Matchers::WithinAbs(-90., 0.001) );
	CHECK_THAT( std::abs(AccessFormat::RelativeDegrees(facingUp, Point(0., 100.))),
		Catch::Matchers::WithinAbs(180., 0.001) );

	// Facing right (east): a target to the north is on the left.
	Angle facingRight(90.);
	CHECK_THAT( AccessFormat::RelativeDegrees(facingRight, Point(0., -50.)), Catch::Matchers::WithinAbs(-90., 0.001) );
	CHECK_THAT( AccessFormat::RelativeDegrees(facingRight, Point(50., 0.)), Catch::Matchers::WithinAbs(0., 0.001) );
}

TEST_CASE( "Clock positions and spoken bearings", "[accessibility][format]" ) {
	CHECK( AccessFormat::ClockPosition(0.) == 12 );
	CHECK( AccessFormat::ClockPosition(90.) == 3 );
	CHECK( AccessFormat::ClockPosition(-90.) == 9 );
	CHECK( AccessFormat::ClockPosition(180.) == 6 );
	CHECK( AccessFormat::ClockPosition(44.) == 1 );
	CHECK( AccessFormat::ClockPosition(46.) == 2 );

	CHECK( AccessFormat::Bearing(1.) == "dead ahead" );
	CHECK( AccessFormat::Bearing(-179.) == "directly behind" );
	CHECK( AccessFormat::Bearing(12.) == "12 degrees right" );
	CHECK( AccessFormat::Bearing(-20.4) == "20 degrees left" );
	CHECK( AccessFormat::Bearing(90.) == "at 3 o'clock" );
	CHECK( AccessFormat::Bearing(-60.) == "at 10 o'clock" );
}

TEST_CASE( "Compass directions use screen up as north", "[accessibility][format]" ) {
	CHECK( AccessFormat::CompassDirection(Point(0., -1.)) == "north" );
	CHECK( AccessFormat::CompassDirection(Point(1., 0.)) == "east" );
	CHECK( AccessFormat::CompassDirection(Point(0., 1.)) == "south" );
	CHECK( AccessFormat::CompassDirection(Point(-1., -1.)) == "north-west" );
	CHECK( AccessFormat::CompassDirection(Point()) == "here" );
}

TEST_CASE( "Number formatting for speech", "[accessibility][format]" ) {
	CHECK( AccessFormat::Integer(0.) == "0" );
	CHECK( AccessFormat::Integer(999.) == "999" );
	CHECK( AccessFormat::Integer(1000.) == "1,000" );
	CHECK( AccessFormat::Integer(1234567.4) == "1,234,567" );
	CHECK( AccessFormat::Integer(-2500.) == "minus 2,500" );
	CHECK( AccessFormat::Credits(1) == "1 credit" );
	CHECK( AccessFormat::Credits(480000) == "480,000 credits" );
	CHECK( AccessFormat::Distance(1.) == "1 unit" );
	CHECK( AccessFormat::Distance(1250.4) == "1,250 units" );
	CHECK( AccessFormat::Percent(.756) == "76 percent" );
	CHECK( AccessFormat::Percent(-1.) == "0 percent" );
	CHECK( AccessFormat::Plural(1, "ton") == "1 ton" );
	CHECK( AccessFormat::Plural(3, "ton") == "3 tons" );
	CHECK( AccessFormat::Plural(2, "body", "bodies") == "2 bodies" );
}

TEST_CASE( "Lists and sentences", "[accessibility][format]" ) {
	CHECK( AccessFormat::List({}) == "" );
	CHECK( AccessFormat::List({"a"}) == "a" );
	CHECK( AccessFormat::List({"a", "b"}) == "a and b" );
	CHECK( AccessFormat::List({"a", "b", "c"}) == "a, b, and c" );
	CHECK( AccessFormat::Sentence("moving forward") == "Moving forward" );
	CHECK( AccessFormat::Sentence("") == "" );
	CHECK( AccessFormat::TrimPeriod("Done.") == "Done" );
	CHECK( AccessFormat::FriendlyKey("Return") == "Enter" );
}

TEST_CASE( "Motion descriptions", "[accessibility][format]" ) {
	Angle facingUp(0.);
	CHECK( AccessFormat::Motion(facingUp, Point(), 0.) == "stationary" );
	CHECK( AccessFormat::Motion(facingUp, Point(0., -1.), 120.) == "moving forward at 120 units per second" );
	CHECK( AccessFormat::Motion(facingUp, Point(1., 0.), 60.) == "drifting right at 60 units per second" );
	CHECK( AccessFormat::Motion(facingUp, Point(0., 1.), 60.) == "moving backward at 60 units per second" );
	CHECK( AccessFormat::Motion(facingUp, Point(-1., -1.), 60.)
		== "moving forward and drifting left at 60 units per second" );
}

TEST_CASE( "Speech text cleaning", "[accessibility][speech]" ) {
	CHECK( AccessSpeech::Clean("  Hello   world  ") == "Hello world" );
	CHECK( AccessSpeech::Clean("Line one\n\nLine two") == "Line one. Line two" );
	CHECK( AccessSpeech::Clean("Tab\tseparated") == "Tab separated" );
	CHECK( AccessSpeech::Clean("_underlined_ text") == "underlined text" );
	CHECK( AccessSpeech::Clean("") == "" );
}

TEST_CASE( "Text field echo", "[accessibility][text]" ) {
	CHECK( AccessText::EchoEdit("Ad", "Ada") == "a" );
	CHECK( AccessText::EchoEdit("Ada", "Ad") == "deleted a" );
	CHECK( AccessText::EchoEdit("Ada", "Ada ") == "space" );
	CHECK( AccessText::EchoEdit("Ada", "") == "cleared" );
	CHECK( AccessText::EchoEdit("Ada", "Ada") == "" );
	CHECK( AccessText::EchoEdit("", "pasted text") == "pasted text" );
	CHECK( AccessText::EchoEdit("", "J") == "cap J" );
	CHECK( AccessText::EchoEdit("J", "Jo") == "o" );
	CHECK( AccessText::IsTypingKey('a') );
	CHECK_FALSE( AccessText::IsTypingKey(0x40000052) );
}

TEST_CASE( "Paragraph splitting", "[accessibility][text]" ) {
	std::vector<std::string> paragraphs = AccessText::Paragraphs("First.\n\n  \nSecond line\nThird");
	REQUIRE( paragraphs.size() == 3 );
	CHECK( paragraphs[0] == "First." );
	CHECK( paragraphs[1] == "Second line" );
	CHECK( paragraphs[2] == "Third" );
}

TEST_CASE( "Captured screen text is grouped into lines", "[accessibility][capture]" ) {
	// A fake panel pointer is only used as a key; it is never dereferenced.
	const Panel *panel = reinterpret_cast<const Panel *>(0x1000);
	Color white(1.f, 1.f);
	AccessCapture::BeginFrame();
	AccessCapture::SetPanel(panel);
	AccessCapture::Text("Food", 100., 50., white);
	AccessCapture::Text("335", 200., 51., white);
	AccessCapture::Text("Title", 100., 10., white);
	AccessCapture::Button("Buy", 'b', true, 300., 48., 50., 20.);
	AccessCapture::Text("Buy", 305., 50., white);
	// Suppressed text is ignored.
	AccessCapture::Suppress(true);
	AccessCapture::Text("hidden", 0., 0., white);
	AccessCapture::Suppress(false);
	AccessCapture::SetPanel(nullptr);
	AccessCapture::Text("outside any panel", 0., 0., white);
	AccessCapture::EndFrame();

	std::vector<AccessCapture::Item> lines = AccessCapture::Lines(panel);
	REQUIRE( lines.size() == 3 );
	CHECK( lines[0].text == "Title" );
	CHECK( lines[1].text == "Food, 335" );
	CHECK( lines[2].text == "Buy" );
	CHECK( lines[2].isButton );
	CHECK( lines[2].key == 'b' );
	CHECK( AccessCapture::Buttons(panel).size() == 1 );
	CHECK( AccessCapture::Panels().size() == 1 );
}
// #endregion unit tests



} // test namespace

TEST_CASE( "Formation messages are recognised", "[accessibility]" ) {
	CHECK( AccessFlight::FormationDescription("Vic") == "a V shape, two legs angled back behind you" );
	CHECK( AccessFlight::FormationDescription("unknown").empty() );
	CHECK( AccessFlight::OnGameMessage("2 ships will assume \"Vic\" formation.") );
	CHECK( AccessFlight::OnGameMessage("1 ship will no longer fly in formation.") );
}

