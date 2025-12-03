//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003-2007 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Date created:  8/19/07
//
//
// Filename: cl_statistics.cpp
//
// Description: Keeps track of the amount of data sent to and from the server,
// and possibly other related things.
//
//-----------------------------------------------------------------------------

#include "cl_statistics.h"
#include "doomtype.h"
#include "stats.h"
#include "doomdef.h"
#include "doomstat.h"
#include "v_palette.h"
#include "v_video.h"

//*****************************************************************************
//	CLASSES

// [BB]
class StatTracker
{
	unsigned int _value;
	unsigned int _valueThisTick;
	unsigned int _valueThisSecond;
	unsigned int _valueLastSecond;
	unsigned int _maxValuePerSecond;

	// [AK] Values for compressed stats.
	unsigned int _compressedValueThisTick;
	unsigned int _compressedValueThisSecond;
	unsigned int _compressedValueLastSecond;

	unsigned int _packetsThisTick;
	unsigned int _packetsThisSecond;
	unsigned int _packetsLastSecond;

	unsigned int _numZstdThisTick;
	unsigned int _numZstdThisSecond;
	unsigned int _numZstdLastSecond;

	// [AK] Used for drawing the net graph.
	unsigned int _maxValueLastMinute;
	RingBuffer<unsigned int, MINUTE> _valuesLastMinute;

public:
	StatTracker ( )
	{
		Clear ( );
	}

	void Clear ( )
	{
		_value = 0;
		_valueThisTick = 0;
		_valueThisSecond = 0;
		_valueLastSecond = 0;
		_maxValuePerSecond = 0;
		_compressedValueThisTick = 0;
		_compressedValueThisSecond = 0;
		_compressedValueLastSecond = 0;
		_packetsThisTick = 0;
		_packetsThisSecond = 0;
		_packetsLastSecond = 0;
		_numZstdThisTick = 0;
		_numZstdThisSecond = 0;
		_numZstdLastSecond = 0;
		_maxValueLastMinute = 0;
		_valuesLastMinute.clear( );
	}

	void AddToTic ( const int Value, const int CompressedValue = 0, const bool WasZstd = false )
	{
		_valueThisTick += Value;

		// [AK] Add to the compressed value for this tick.
		_compressedValueThisTick += CompressedValue;

		_packetsThisTick++;
		if (WasZstd)
			_numZstdThisTick++;
	}

	void TicPassed ( )
	{
		_value += _valueThisTick;
		_valueThisSecond += _valueThisTick;
		_valueThisTick = 0;

		// [AK] Add the compressed value from this tick to this second too.
		_compressedValueThisSecond += _compressedValueThisTick;
		_compressedValueThisTick = 0;

		_packetsThisSecond += _packetsThisTick;
		_packetsThisTick = 0;

		_numZstdThisSecond += _numZstdThisTick;
		_numZstdThisTick = 0;
	}

	void SecondPassed ( )
	{
		_valueLastSecond = _valueThisSecond;
		_valueThisSecond = 0;

		if ( _maxValuePerSecond < _valueLastSecond )
			_maxValuePerSecond = _valueLastSecond;

		// [AK] Save the compressed value from this second too.
		_compressedValueLastSecond = _compressedValueThisSecond;
		_compressedValueThisSecond = 0;

		_packetsLastSecond = _packetsThisSecond;
		_packetsThisSecond = 0;

		_numZstdLastSecond = _numZstdThisSecond;
		_numZstdThisSecond = 0;

		// [AK] Add the uncompressed value from this last second to the buffer of
		// values in the last minute, then determine the new maximum value.
		_valuesLastMinute.put( _valueLastSecond );
		_maxValueLastMinute = 0;

		for ( unsigned int i = 0; i < MINUTE; i++ )
			_maxValueLastMinute = MAX<unsigned>( _maxValueLastMinute, _valuesLastMinute.getOldestEntry( i ));
	}

	unsigned int getTotalValue ( ) const
	{
		return _value;
	}

	unsigned int getValueThisTick ( ) const
	{
		return _valueThisTick;
	}

	unsigned int getValueLastSecond ( ) const
	{
		return _valueLastSecond;
	}

	unsigned int getMaxValuePerSecond ( ) const
	{
		return _maxValuePerSecond;
	}

	// [AK] Returns a percentage of compressed to uncompressed stats.
	float getCompressionFactorLastSecond ( ) const
	{
		float percentage = 100.0f;

		if ( _compressedValueLastSecond != _valueLastSecond )
			percentage *= static_cast<float>( _compressedValueLastSecond ) / static_cast<float>( _valueLastSecond );

		return percentage;
	}

	float getZstdPercentageLastSecond ( ) const
	{
		float percentage = 100.0f;

		if ( _numZstdLastSecond != _packetsLastSecond )
			percentage *= static_cast<float>( _numZstdLastSecond ) / static_cast<float>( _packetsLastSecond );

		return percentage;
	}

	// [AK] Returns the largest uncompressed value in the last minute.
	unsigned int getMaxValueLastMinute ( ) const
	{
		return _maxValueLastMinute;
	}

	// [AK] Returns any uncompressed value in the last minute, from 0-59 seconds.
	unsigned int getValueLastMinute ( unsigned int second ) const
	{
		return ( second < MINUTE ? _valuesLastMinute.getOldestEntry( second ) : 0 );
	}
};

//*****************************************************************************
// [AK]
class NetGraph
{
public:
	NetGraph( const char *label, const char *scaleLabel, unsigned int scaleDivider, unsigned int defaultScaleMax, unsigned int scaleBonus ) :
		label( label ),
		scaleLabel( scaleLabel ),
		scaleDivider( scaleDivider > 0 ? scaleDivider : 1 ),
		defaultScaleMax( defaultScaleMax ),
		scaleBonus( scaleBonus ) { }

	void AddLine( const StatTracker &tracker, const char *name, PalEntry color )
	{
		lines.Push( { tracker, name, color } );
	}

	void Draw( const int leftX, const int topY ) const
	{
		const int rightX = leftX + WIDTH;
		const int bottomY = topY + HEIGHT;
		const int gridColor = MAKERGB( 48, 48, 48 );
		const int borderColor = MAKERGB( 64, 64, 64 );
		unsigned int scaleMax = defaultScaleMax;
		int textXPos = 0;
		int textYPos = 0;
		int textYOffset = 0;
		FString scaleText = '0';

		// [AK] Determine the maximum value to be used on the scale.
		for ( unsigned int i = 0; i < lines.Size( ); i++ )
			scaleMax = MAX<unsigned>( scaleMax, lines[i].tracker.getMaxValueLastMinute( ) + scaleBonus );

		// [AK] Draw the background of the net graph first.
		screen->Dim( MAKERGB( 16, 16, 16 ), 0.65f, leftX, topY, WIDTH, HEIGHT );

		// [AK] Next, draw the gridlines. The vertical lines move left as time goes.
		const int secondsBetweenVerticalGridLines = MINUTE / 6;
		int verticalGridLineX = leftX - (( gametic / TICRATE ) % ( secondsBetweenVerticalGridLines )) * TICK_WIDTH;
		int horizontalGridLineY = topY;

		do
		{
			verticalGridLineX += TICK_WIDTH * secondsBetweenVerticalGridLines;

			if ( verticalGridLineX >= rightX )
				break;

			screen->DrawLine( verticalGridLineX, topY, verticalGridLineX, bottomY, -1, gridColor );

		} while ( true );

		do
		{
			horizontalGridLineY += HEIGHT / 4;

			if ( horizontalGridLineY >= bottomY )
				break;

			screen->DrawLine( leftX, horizontalGridLineY, rightX, horizontalGridLineY, -1, gridColor );

		} while ( true );

		// [AK] Draw each line to be plotted on the graph.
		for ( unsigned int i = 0; i < lines.Size( ); i++ )
		{
			int lineXPos1 = 0;
			int lineYPos1 = 0;

			for ( unsigned int j = 0; j < MINUTE; j++ )
			{
				int lineXPos2 = leftX + TICK_WIDTH * j;
				int lineYPos2 = bottomY - static_cast<int>(( lines[i].tracker.getValueLastMinute( j ) * HEIGHT ) / scaleMax );

				if ( j > 0 )
					screen->DrawLine( lineXPos1, lineYPos1, lineXPos2, lineYPos2, -1, lines[i].color );

				lineXPos1 = lineXPos2;
				lineYPos1 = lineYPos2;
			}
		}

		// [AK] If this net graph has more than one line to draw, add a legend.
		if ( lines.Size( ) > 1 )
		{
			int legendXPos = leftX + LEGEND_SPACING;
			int legendYPos = topY + LEGEND_SPACING;

			for ( unsigned int i = 0; i < lines.Size( ); i++ )
			{
				if ( lines[i].name.IsEmpty( ))
					continue;

				const int legendTextWidth = ConFont->StringWidth( lines[i].name.GetChars( ));
				const int totalLegendWidth = legendTextWidth + LEGEND_LINE_WIDTH + LEGEND_SPACING;

				// [AK] Check if there's enough room to draw this line's legend.
				if ( legendXPos + totalLegendWidth > rightX - LEGEND_SPACING )
				{
					// [AK] If the legend is somehow too big to fit in the net graph,
					// just continue to the next line instead.
					if ( totalLegendWidth + 2 * LEGEND_SPACING > WIDTH )
						continue;

					legendXPos = leftX + LEGEND_SPACING;
					legendYPos += ConFont->GetHeight( ) + 2;

					// [AK] Stop if there's no vertical space left to fit more legends.
					if ( legendYPos > bottomY - LEGEND_SPACING )
						break;
				}

				screen->DrawLine( legendXPos, legendYPos, legendXPos + LEGEND_LINE_WIDTH, legendYPos, -1, lines[i].color );
				legendXPos += LEGEND_LINE_WIDTH + LEGEND_SPACING;

				int newYPos = legendYPos - ConFont->StringHeight( lines[i].name.GetChars( ), &textYOffset ) / 2;

				screen->DrawText( ConFont, CR_GREY, legendXPos, newYPos - textYOffset, lines[i].name.GetChars( ), TAG_DONE );
				legendXPos += ConFont->StringWidth( lines[i].name.GetChars( )) + LEGEND_SPACING;
			}
		}

		// [AK] Draw the borders around the graph after drawing the lines.
		screen->DrawLine( leftX, topY, rightX, topY, -1, borderColor );
		screen->DrawLine( leftX, bottomY, rightX, bottomY, -1, borderColor );
		screen->DrawLine( leftX, topY, leftX, bottomY, -1, borderColor );
		screen->DrawLine( rightX, topY, rightX, bottomY, -1, borderColor );

		// [AK] Draw the graph's label in the top-center, if applicable.
		if ( label.IsNotEmpty( ))
		{
			textXPos = leftX + ( WIDTH - ConFont->StringWidth( label.GetChars( ))) / 2;
			textYPos = topY - ConFont->StringHeight( label.GetChars( ), &textYOffset ) - 2;

			screen->DrawText( ConFont, CR_WHITE, textXPos, textYPos - textYOffset, label.GetChars( ), TAG_DONE );
		}

		// [AK] Draw the min and max scale values on the right, and the label if there is one.
		textXPos = rightX + 2;
		textYPos = bottomY - ConFont->StringHeight( scaleText.GetChars( ), &textYOffset );
		screen->DrawText( ConFont, CR_WHITE, textXPos, textYPos - textYOffset, scaleText.GetChars( ), TAG_DONE );

		scaleText.Format( "%u", scaleDivider > 1 ? scaleMax / scaleDivider : scaleMax );

		if ( scaleLabel.IsNotEmpty( ))
			scaleText.AppendFormat( " %s", scaleLabel.GetChars( ));

		screen->DrawText( ConFont, CR_WHITE, textXPos, topY, scaleText.GetChars( ), TAG_DONE );
	}

	// [AK] Static constants for the net graph.
	static const int TICK_WIDTH = 8;
	static const int WIDTH = TICK_WIDTH * ( MINUTE - 1 );
	static const int HEIGHT = 120;

	static const int LEGEND_LINE_WIDTH = 16;
	static const int LEGEND_SPACING = 10;

private:
	struct Line
	{
		const StatTracker &tracker;
		FString name;
		PalEntry color;
	};

	TArray<Line> lines;
	const FString label;
	const FString scaleLabel;
	const unsigned int scaleDivider;
	const unsigned int defaultScaleMax;
	const unsigned int scaleBonus;
};

//*****************************************************************************
//	VARIABLES

static	StatTracker			g_bytesSentStatTracker;
static	StatTracker			g_bytesReceivedStatTracker;
static	StatTracker			g_missingPacketsRequestedStatTracker;

static	NetGraph			g_NetTrafficGraph( "Network Traffic", "KB/s", 1000, 6000, 2000 );
static	NetGraph			g_MissingPacketsGraph( "Missing Packets", nullptr, 1, 10, 2 );

//*****************************************************************************
//	CONSOLE VARIABLES

// [AK] If enabled, shows the net graph when the "nettraffic" stat is active.
CVAR( Bool, cl_netgraph, false, CVAR_ARCHIVE | CVAR_NOSETBYACS )

//*****************************************************************************
//	FUNCTIONS

void CLIENTSTATISTICS_Construct( void )
{
	g_bytesSentStatTracker.Clear();
	g_bytesReceivedStatTracker.Clear();
	g_missingPacketsRequestedStatTracker.Clear();

	// [AK] Add each of the lines to their respective net graphs.
	g_NetTrafficGraph.AddLine( g_bytesReceivedStatTracker, "In", MAKERGB( 255, 20, 35 ));
	g_NetTrafficGraph.AddLine( g_bytesSentStatTracker, "Out", MAKERGB( 100, 255, 55 ));
	g_MissingPacketsGraph.AddLine( g_missingPacketsRequestedStatTracker, "Missing", MAKERGB( 255, 20, 35 ));
}

//*****************************************************************************
//
void CLIENTSTATISTICS_Tick( void )
{
	// Add to the number of bytes sent/received this second.
	g_bytesSentStatTracker.TicPassed();
	g_bytesReceivedStatTracker.TicPassed();
	g_missingPacketsRequestedStatTracker.TicPassed();

	// Every second, update the number of bytes sent last second with the number of bytes
	// sent this second, and then reset the number of bytes sent this second.
	if (( gametic % TICRATE ) == 0 )
	{
		g_bytesSentStatTracker.SecondPassed ( );
		g_bytesReceivedStatTracker.SecondPassed ( );
		g_missingPacketsRequestedStatTracker.SecondPassed ( );
	}
}

//*****************************************************************************
//
void CLIENTSTATISTICS_AddToBytesSent( unsigned int uncompressedBytes, unsigned int compressedBytes, bool wasZstdPacket )
{
	g_bytesSentStatTracker.AddToTic ( uncompressedBytes, compressedBytes, wasZstdPacket );
}

//*****************************************************************************
//
void CLIENTSTATISTICS_AddToBytesReceived( unsigned int uncompressedBytes, unsigned int compressedBytes, bool wasZstdPacket )
{
	g_bytesReceivedStatTracker.AddToTic ( uncompressedBytes, compressedBytes, wasZstdPacket );
}

//*****************************************************************************
//
void CLIENTSTATISTICS_AddToMissingPacketsRequested( unsigned int Num )
{
	g_missingPacketsRequestedStatTracker.AddToTic ( Num );
}

//*****************************************************************************
//
void CLIENTSTATISTICS_DrawNetGraph( int xPos, int yPos )
{
	// [AK] Move the top of the first net graph by an extra four pixels.
	int newYPos = yPos - ( NetGraph::HEIGHT + 4 );

	if ( cl_netgraph )
	{
		g_NetTrafficGraph.Draw( xPos, newYPos );

		// [AK] Separate both net graphs by an extra eight pixels.
		newYPos -= ( NetGraph::HEIGHT + 8 + ConFont->GetHeight( ));
		g_MissingPacketsGraph.Draw( xPos, newYPos );
	}
}

//*****************************************************************************
//	STATISTICS

ADD_STAT( nettraffic )
{
	FString out;

	out.Format( "In:   %5u/%5u/%5u (comp: %.1f%%, zstd: %.1f%%)\nOut:  %5u/%5u/%5u (comp: %.1f%%, zstd: %.1f%%)\nLoss: %5u/%5u",
		g_bytesReceivedStatTracker.getValueThisTick( ),
		g_bytesReceivedStatTracker.getValueLastSecond( ),
		g_bytesReceivedStatTracker.getMaxValuePerSecond( ),
		g_bytesReceivedStatTracker.getCompressionFactorLastSecond( ),
		g_bytesReceivedStatTracker.getZstdPercentageLastSecond(),
		g_bytesSentStatTracker.getValueThisTick( ),
		g_bytesSentStatTracker.getValueLastSecond( ),
		g_bytesSentStatTracker.getMaxValuePerSecond( ),
		g_bytesSentStatTracker.getCompressionFactorLastSecond( ),
		g_bytesSentStatTracker.getZstdPercentageLastSecond(),
		g_missingPacketsRequestedStatTracker.getValueLastSecond( ),
		g_missingPacketsRequestedStatTracker.getMaxValuePerSecond( ));

	return out;
}
