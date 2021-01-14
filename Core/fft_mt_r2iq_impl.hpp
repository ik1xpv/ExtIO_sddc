
{
	const int decimate = this->mdecimation;
	const int mfft = this->mfftdim[decimate];	// = halfFft / 2^mdecimation
	const int mratio = this->getRatio();
	const fftwf_complex* filter = filterHw[decimate];
	const bool lsb = this->getSideband();
	plan_f2t_c2c = &plans_f2t_c2c[decimate];

	while (r2iqOn) {
		const int16_t *dataADC;  // pointer to input data
		const float *endloop;    // pointer to end data to be copied to beginning
		fftwf_complex* pout;

		const int _mtunebin = this->mtunebin;  // Update LO tune is possible during run

		{
			int wakecnt = 0;
			std::unique_lock<std::mutex> lk(mutexR2iqControl);
			while (this->cntr <= 0)
			{
				cvADCbufferAvailable.wait(lk, [&wakecnt, this] {
					wakecnt++;
					return this->cntr > 0;
				});
			}

			if (!r2iqOn)
				return 0;

			dataADC = this->buffers[this->bufIdx];
			std::atomic_fetch_sub(&this->cntr, 1);

			int modx = this->bufIdx / mratio;
			int moff = this->bufIdx - modx * mratio;
			int offset = ((transferSize / sizeof(int16_t)) / mratio) * moff;
			pout = (fftwf_complex*)(this->obuffers[modx] + offset);

			this->bufIdx = (this->bufIdx + 1) % QUEUE_SIZE;

			endloop = lastThread->ADCinTime + transferSize / sizeof(int16_t);
			lastThread = th;
		}

		// first frame
		auto inloop = th->ADCinTime;
		for (int m = 0; m < halfFft; m++) {
			inloop[m] = endloop[m];   // duplicate from last frame halfFft samples
		}
		inloop += halfFft;

		// @todo: move the following int16_t conversion to (32-bit) float
		// directly inside the following loop (for "k < fftPerBuf")
		//   just before the forward fft "fftwf_execute_dft_r2c" is called
		// idea: this should improve cache/memory locality
#if PRINT_INPUT_RANGE
		std::pair<int16_t, int16_t> blockMinMax = std::make_pair<int16_t, int16_t>(0, 0);
#endif
		if (!this->getRand())        // plain samples no ADC rand set
		{
#if PRINT_INPUT_RANGE
			auto minmax = std::minmax_element(dataADC, dataADC + transferSamples);
			blockMinMax.first = *minmax.first;
			blockMinMax.second = *minmax.second;
#endif
			convert_float<false>(dataADC, inloop, transferSamples);
		}
		else
		{
			convert_float<true>(dataADC, inloop, transferSamples);
		}

#if PRINT_INPUT_RANGE
		th->MinValue = std::min(blockMinMax.first, th->MinValue);
		th->MaxValue = std::max(blockMinMax.second, th->MaxValue);
		++th->MinMaxBlockCount;
		if (th->MinMaxBlockCount * processor_count / 3 >= DEFAULT_TRANSFERS_PER_SEC )
		{
			float minBits = (th->MinValue < 0) ? (log10f((float)(-th->MinValue)) / log10f(2.0f)) : -1.0f;
			float maxBits = (th->MaxValue > 0) ? (log10f((float)(th->MaxValue)) / log10f(2.0f)) : -1.0f;
			printf("r2iq: min = %d (%.1f bits) %.2f%%, max = %d (%.1f bits) %.2f%%\n",
				(int)th->MinValue, minBits, th->MinValue *-100.0f / 32768.0f,
				(int)th->MaxValue, maxBits, th->MaxValue * 100.0f / 32768.0f);
			th->MinValue = 0;
			th->MaxValue = 0;
			th->MinMaxBlockCount = 0;
		}
#endif

		// decimate in frequency plus tuning

		// Calculate the parameters for the first half
		const auto count = std::min(mfft/2, halfFft - _mtunebin);
		const auto source = &th->ADCinFreq[_mtunebin];

		// Calculate the parameters for the second half
		const auto start = std::max(0, mfft / 2 - _mtunebin);
		const auto source2 = &th->ADCinFreq[_mtunebin - mfft / 2];
		const auto filter2 = &filter[halfFft - mfft / 2];
		const auto dest = &th->inFreqTmp[mfft / 2];
		for (int k = 0; k < fftPerBuf; k++)
		{
			// core of fast convolution including filter and decimation
			//   main part is 'overlap-scrap' (IMHO better name for 'overlap-save'), see
			//   https://en.wikipedia.org/wiki/Overlap%E2%80%93save_method
			{
				// FFT first stage: time to frequency, real to complex
				// 'full' transformation size: 2 * halfFft
				fftwf_execute_dft_r2c(plan_t2f_r2c, th->ADCinTime + (3 * halfFft / 2) * k, th->ADCinFreq);
				// result now in th->ADCinFreq[]

				// circular shift (mixing in full bins) and low/bandpass filtering (complex multiplication)
				{
					// circular shift tune fs/2 first half array into th->inFreqTmp[]
					shift_freq(th->inFreqTmp, source, filter, 0, count);
					if (mfft / 2 != count)
						memset(th->inFreqTmp[count], 0, sizeof(float) * 2 * (mfft / 2 - count));

					// circular shift tune fs/2 second half array
					shift_freq(dest, source2, filter2, start, mfft/2);
					if (start != 0)
						memset(th->inFreqTmp[mfft / 2], 0, sizeof(float) * 2 * start);
				}
				// result now in th->inFreqTmp[]

				// 'shorter' inverse FFT transform (decimation); frequency (back) to COMPLEX time domain
				// transform size: mfft = mfftdim[k] = halfFft / 2^k with k = mdecimation
				fftwf_execute_dft(*plan_f2t_c2c, th->inFreqTmp, th->outTimeTmp);     //  c2c decimation
				// result now in th->outTimeTmp[]
			}

			// postprocessing
			// @todo: is it possible to ..
			//  1)
			//    let inverse FFT produce/save it's result directly
			//    in "this->obuffers[modx] + offset" (pout)
			//    ( obuffers[] would need to have additional space ..;
			//      need to move 'scrap' of 'ovelap-scrap'? )
			//    at least FFTW would allow so,
			//      see http://www.fftw.org/fftw3_doc/New_002darray-Execute-Functions.html
			//    attention: multithreading!
			//  2)
			//    could mirroring (lower sideband) get calculated together
			//    with fine mixer - modifying the mixer frequency? (fs - fc)/fs
			//    (this would reduce one memory pass)
			if (lsb) // lower sideband
			{
				// mirror just by negating the imaginary Q of complex I/Q
				if (k == 0)
				{
					copy<true>(pout, &th->outTimeTmp[mfft / 4], mfft/2);
				}
				else
				{
					copy<true>(pout + mfft / 2 + (3 * mfft / 4) * (k - 1), &th->outTimeTmp[0], (3 * mfft / 4));
				}
			}
			else // upper sideband
			{
				if (k == 0)
				{
					copy<false>(pout, &th->outTimeTmp[mfft / 4], mfft/2);
				}
				else
				{
					copy<false>(pout + mfft / 2 + (3 * mfft / 4) * (k - 1), &th->outTimeTmp[0], (3 * mfft / 4));
				}
			}
			// result now in this->obuffers[]
		}
	} // while(run)
//    DbgPrintf((char *) "r2iqThreadf idx %d pthread_exit %u\n",(int)th->t, pthread_self());
	return 0;
}
