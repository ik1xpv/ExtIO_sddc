
{
	while (r2iqOn)
	{
		const int16_t *dataADC; // pointer to input data
		const int16_t *endloop; // pointer to end data to be copied to beginning

		dataADC = inputbuffer->getReadPtr();

		if (!r2iqOn)
			return 0;

		this->bufIdx = (this->bufIdx + 1) % QUEUE_SIZE;

		endloop = inputbuffer->peekReadPtr(-1) + transferSamples - halfFft;

		auto inloop = this->ADCinTime;

		// @todo: move the following int16_t conversion to (32-bit) float
		// directly inside the following loop (for "k < fftPerBuf")
		//   just before the forward fft "fftwf_execute_dft_r2c" is called
		// idea: this should improve cache/memory locality
#if PRINT_INPUT_RANGE
		std::pair<int16_t, int16_t> blockMinMax = std::make_pair<int16_t, int16_t>(0, 0);
#endif
		if (!this->getRand()) // plain samples no ADC rand set
		{
			convert_float<false>(endloop, inloop, halfFft);
#if PRINT_INPUT_RANGE
			auto minmax = std::minmax_element(dataADC, dataADC + transferSamples);
			blockMinMax.first = *minmax.first;
			blockMinMax.second = *minmax.second;
#endif
			convert_float<false>(dataADC, inloop + halfFft, transferSamples);
		}
		else
		{
			convert_float<true>(endloop, inloop, halfFft);
			convert_float<true>(dataADC, inloop + halfFft, transferSamples);
		}

#if PRINT_INPUT_RANGE
		this->MinValue = std::min(blockMinMax.first, this->MinValue);
		this->MaxValue = std::max(blockMinMax.second, this->MaxValue);
		++this->MinMaxBlockCount;
		if (this->MinMaxBlockCount * processor_count / 3 >= DEFAULT_TRANSFERS_PER_SEC)
		{
			float minBits = (this->MinValue < 0) ? (log10f((float)(-this->MinValue)) / log10f(2.0f)) : -1.0f;
			float maxBits = (this->MaxValue > 0) ? (log10f((float)(this->MaxValue)) / log10f(2.0f)) : -1.0f;
			printf("r2iq: min = %d (%.1f bits) %.2f%%, max = %d (%.1f bits) %.2f%%\n",
				   (int)this->MinValue, minBits, this->MinValue * -100.0f / 32768.0f,
				   (int)this->MaxValue, maxBits, this->MaxValue * 100.0f / 32768.0f);
			this->MinValue = 0;
			this->MaxValue = 0;
			this->MinMaxBlockCount = 0;
		}
#endif
		dataADC = nullptr;
		inputbuffer->ReadDone();

		for (int k = 0; k < fftPerBuf; k++)
		{
			fftwf_complex *ADCinFreq;         // buffers in frequency

			// core of fast convolution including filter and decimation
			//   main part is 'overlap-scrap' (IMHO better name for 'overlap-save'), see
			//   https://en.wikipedia.org/wiki/Overlap%E2%80%93save_method
			ADCinFreq = freqdomain.getWritePtr();

			// FFT first stage: time to frequency, real to complex
			// 'full' transformation size: 2 * halfFft
			fftwf_execute_dft_r2c(plan_t2f_r2c, this->ADCinTime + (3 * halfFft / 2) * k, ADCinFreq);
			// result now in ADCinFreq[]
			freqdomain.WriteDone();
		}
	}

	return 0;
}
