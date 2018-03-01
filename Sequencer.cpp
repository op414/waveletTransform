#include "Sequencer.h"

#define SEQ_STATE_READY 0
#define SEQ_STATE_HORIZONTAL_TRANSFORM 1
#define SEQ_STATE_VERTICAL_TRANSFORM 2

Sequencer::Sequencer(sc_module_name name, int nbRows, int nbCols) : 
	sc_module(name), nbRows(nbRows), nbCols(nbCols)
{	
	// Sequencer logic is falling edge triggered
	SC_METHOD(update_state);
		sensitive << clk.neg() << reset;

	SC_METHOD(compute_outputs);
		sensitive << globalState << currentRow << currentCol;
}

void Sequencer::update_state()
{
	// Reset
	if(reset.read() == false)
	{
		globalState.write(SEQ_STATE_READY);
	}
	// Start transform
	else if(globalState.read() == SEQ_STATE_READY && start.read() == true)
	{
		globalState.write(SEQ_STATE_HORIZONTAL_TRANSFORM);
		currentRow.write(0);
		currentCol.write(-2);
	}
	// Computing
	else if(globalState.read() == SEQ_STATE_HORIZONTAL_TRANSFORM)
	{
		currentCol.write(currentCol.read() + 1);

		// Row over, switch to next one
		if(currentCol.read() == nbCols + 5)
		{
			currentRow.write(currentRow.read() + 1);
			currentCol.write(-2);
			
			// Image over
			if(currentRow.read() == nbRows - 1)
			{
				globalState.write(SEQ_STATE_VERTICAL_TRANSFORM);
			}
		}
	}
	else if(globalState.read() == SEQ_STATE_VERTICAL_TRANSFORM)
	{
		currentRow.write(currentRow.read() + 1);

		// Column over, switch to next one
		if(currentRow.read() == nbRows + 5)
		{
			currentCol.write(currentCol.read() + 1);
			currentRow.write(-2);
			
			// Image over
			if(currentCol.read() == nbCols - 1)
			{
				globalState.write(SEQ_STATE_READY);
			}
		}
	}
}

void Sequencer::compute_outputs()
{
	if(globalState.read() == SEQ_STATE_READY)
	{
		busy.write(false);
		mem_in_addr.write(0);
		mem_out_addr.write(0);
		mem_out_write.write(false);
		first_c.write(false);
		even.write(true);
	}
	else // Computing
	{
		busy.write(true);

		// Horizontal transsform
		sc_signal<int>* primDim = &currentCol;
		int nbPrimDim = nbCols;
		
		// Vertical transform
		if(globalState.read() == SEQ_STATE_VERTICAL_TRANSFORM)
		{
			primDim = &currentRow;
			nbPrimDim = nbRows;
		}

		// Manage image folding on the sides
		int actualPrimDim = primDim->read();
		if(actualPrimDim < 0)
			actualPrimDim = -actualPrimDim;
		else if(actualPrimDim > nbPrimDim)
			actualPrimDim = nbPrimDim - (actualPrimDim - nbPrimDim);

		// Input address to access
		int addrIn = actualPrimDim + nbCols * currentRow.read();
		
		if(globalState.read() == SEQ_STATE_VERTICAL_TRANSFORM)
			addrIn = currentCol.read() + nbCols * actualPrimDim + nbRows * nbCols;

		mem_in_addr.write(addrIn);

		// Even or odd pixel loading
		bool evenPrimDim = actualPrimDim % 2 == 0;
		even.write(evenPrimDim);

		// Use C0 or Cj formula for C
		if(currentCol.read() == 5)
			first_c.write(true);
		else
			first_c.write(false);

		// Output address to access
		// Inputing even column or row, the transform blocks outputs D value
		if(evenPrimDim)
		{	
			// The first D value is obtained when the even input pixel index is >= 6
			if(primDim->read() >= 6)
			{
				if(globalState.read() == SEQ_STATE_HORIZONTAL_TRANSFORM)
				{
					mem_out_addr.write(
						nbCols * nbRows // Output memory is second third of memory
						+ nbCols * currentRow.read() // Position on right row
						+ nbCols / 2 // Offset by half the image
						+ (currentCol.read() - 6) / 2 // Column position
					);
				}
				else // SEQ_STATE_VERTICAL_TRANSFORM
				{
					mem_out_addr.write(
						2 * nbCols * nbRows // Output memory is last third of memory
						+ currentCol.read() // Postion on right column
						+ (currentRow.read() - 6) * nbCols / 2 // Row position
					);
				}
				mem_out_write.write(true);
			}
			else
			{
				mem_out_addr.write(0);
				mem_out_write.write(false);
			}
		}
		else // Inputing odd column or row, the transform block outputs C value
		{
			if(primDim->read() >= 7)
			{
				if(globalState.read() == SEQ_STATE_HORIZONTAL_TRANSFORM)
				{
					mem_out_addr.write(
						nbCols * nbRows // Output memory is second half of memory
						+ nbCols * currentRow.read()  // Position on right row
						+ (currentCol.read() - 7) / 2 // Column position
					);
				}
				else // SEQ_STATE_VERTICAL_TRANSFORM
				{
					mem_out_addr.write(
						2 * nbCols * nbRows // Output memory is last third of memory
						+ currentCol.read() // Position on right column
						+ nbCols* nbRows / 2 // Offset by half the image
						+ (currentRow.read() - 7) * nbCols / 2 // Row position
					);
				}
				mem_out_write.write(true);
			}
			else
			{
				mem_out_addr.write(0);
				mem_out_write.write(false);
			}
		}

	}
}