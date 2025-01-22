

#include "VideoPlayer/VideoPlayer.h"

#include "PcmVolumeControl.h"

#include <stdio.h>

#include <QDebug>
#include <qthread>
#include <chrono>
#include <thread>
#include<algorithm>

#define MAX_AUDIO_BUFFER_SIZE 1024*1024*10

void VideoPlayer::sdlAudioCallBackFunc(void* userdata, Uint8* stream, int len)
{
	VideoPlayer* player = (VideoPlayer*)userdata;
	player->sdlAudioCallBack(stream, len);
}

void VideoPlayer::sdlAudioCallBack(Uint8* stream, int len)
{
    // int len1, audio_data_size;

    // /*   len是由SDL传入的SDL缓冲区的大小，如果这个缓冲未满，我们就一直往里填充数据 */
    // while (len > 0)
    // {
    // 	/*  audio_buf_index 和 audio_buf_size 标示我们自己用来放置解码出来的数据的缓冲区，*/
    // 	/*   这些数据待copy到SDL缓冲区， 当audio_buf_index >= audio_buf_size的时候意味着我*/
    // 	/*   们的缓冲为空，没有数据可供copy，这时候需要调用audio_decode_frame来解码出更
    // 	 /*   多的桢数据 */
    // 	if (audio_buf_index >= audio_buf_size)
    // 	{
    // 		audio_data_size = decodeAudioFrame();

    // 		/* audio_data_size < 0 标示没能解码出数据，我们默认播放静音 */
    // 		if (audio_data_size <= 0)
    // 		{
    // 			/* silence */
    // 			audio_buf_size = 1024;
    // 			/* 清零，静音 */
    // 			memset(audio_buf, 0, audio_buf_size);
    // 		}
    // 		else
    // 		{
    // 			audio_buf_size = audio_data_size;
    // 		}
    // 		audio_buf_index = 0;
    // 	}
    // 	/*  查看stream可用空间，决定一次copy多少数据，剩下的下次继续copy */
    // 	len1 = audio_buf_size - audio_buf_index;

    // 	if (len1 > len)
    // 	{
    // 		len1 = len;
    // 	}

    // 	if (audio_buf == NULL) return;

 //        if (mIsMute || mIsNeedPause) //静音 或者 是在暂停的时候跳转了
 //        {
 //            memset(audio_buf + audio_buf_index, 0, len1);
 //        }
 //        else
 //        {
 //            //音量调节
 //            PcmVolumeControl::RaiseVolume((char*)audio_buf + audio_buf_index, len1, 1, mVolume);
 //        }

 //        memcpy(stream, (uint8_t*)audio_buf + audio_buf_index, len1);

    // 	len -= len1;
    // 	stream += len1;
    // 	audio_buf_index += len1;

    // }
    int len1, audio_data_size;

    // 循环填充 SDL 音频缓冲区
    while (len > 0)
    {
        // 如果 audio_buf 中的数据已经全部使用完，解码新的音频帧
        if (audio_buf_index >= audio_buf_size)
        {
            audio_data_size = decodeAudioFrame();

            // 如果解码失败或没有数据，填充静音
            if (audio_data_size <= 0)
            {
                audio_buf_size = 1024;  // 默认静音缓冲区大小
                memset(audio_buf, 0, audio_buf_size);
            }
            else
            {
                audio_buf_size = audio_data_size;
            }
            audio_buf_index = 0;

            // 如果需要变速播放，将解码后的音频数据写入 sonicStream
            if (mSpeed!=1.0 && sonicStream != nullptr && audio_buf_size > 0)
            {
                std::lock_guard<std::mutex> lock(sonicMutex);  // 确保线程安全

                // 计算实际采样点数
                int actual_out_samples = audio_buf_size / (audio_tgt_channels * av_get_bytes_per_sample(out_sample_fmt));

                // 将数据写入 sonicStream
                if (out_sample_fmt == AV_SAMPLE_FMT_FLT)
                {
                    sonicWriteFloatToStream(sonicStream, (float*)audio_buf, actual_out_samples);
                }
                else if (out_sample_fmt == AV_SAMPLE_FMT_S16)
                {
                    sonicWriteShortToStream(sonicStream, (short*)audio_buf, actual_out_samples);
                }
                else
                {
                    fprintf(stderr, "Unsupported sample format for sonic\n");
                }

                // 从 sonicStream 中读取变速后的数据
                int num_samples = sonicSamplesAvailable(sonicStream);
                int out_size = num_samples * audio_tgt_channels * av_get_bytes_per_sample(out_sample_fmt);

                // 检查 audio_buf 是否足够大
                if (out_size > sizeof(audio_buf))
                {
                    fprintf(stderr, "Audio buffer is too small for resampled data\n");
                    return;
                }

                // 从 sonicStream 中读取数据并存储到 audio_buf
                if (out_sample_fmt == AV_SAMPLE_FMT_FLT)
                {
                    sonicReadFloatFromStream(sonicStream, (float*)audio_buf, num_samples);
                }
                else if (out_sample_fmt == AV_SAMPLE_FMT_S16)
                {
                    sonicReadShortFromStream(sonicStream, (short*)audio_buf, num_samples);
                }

                // 更新 audio_buf_size 和 audio_buf_index
                audio_buf_size = out_size;
                audio_buf_index = 0;
            }
        }

        // 从 audio_buf 中读取数据并填充到 SDL 缓冲区
        len1 = audio_buf_size - audio_buf_index;
        if (len1 > len)
        {
            len1 = len;
        }

        if (audio_buf)
        {
            //memcpy(stream, (uint8_t*)audio_buf + audio_buf_index, len1);
            // 如果静音，填充静音数据（全 0）
            if (mIsMute || mIsNeedPause)
            {
                memset(stream, 0, len1);
            }
            else
            {
                // 正常播放，拷贝音频数据并调节音量
                memcpy(stream, (uint8_t*)audio_buf + audio_buf_index, len1);
                PcmVolumeControl::RaiseVolume((char*)stream, len1, 1, mVolume);
            }
        }
        else
        {
            memset(stream, 0, len1);
        }

        // 更新缓冲区指针和剩余长度
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
}

int VideoPlayer::decodeAudioFrame(bool isBlock)
{
	int audioBufferSize = 0;

	while (1)
	{
		if (mIsQuit)
		{
			mIsAudioThreadFinished = true;
			clearAudioQueue(); //清空队列
			break;
		}

		if (mIsPause == true) //判断暂停
		{
			break;
		}

		mConditon_Audio->Lock();

		if (mAudioPacktList.size() <= 0)
		{
			if (isBlock)
			{
				mConditon_Audio->Wait();
			}
			else
			{
				mConditon_Audio->Unlock();
				break;
			}
		}

		AVPacket packet = mAudioPacktList.front();
		mAudioPacktList.pop_front();
		//qDebug()<<__FUNCTION__<<mAudioPacktList.size();
		mConditon_Audio->Unlock();

		AVPacket* pkt = &packet;

		/* if update, update the audio clock w/pts */
		if (pkt->pts != AV_NOPTS_VALUE)
		{
			audio_clock = av_q2d(mAudioStream->time_base) * pkt->pts;
		}

		//收到这个数据 说明刚刚执行过跳转 现在需要把解码器的数据 清除一下
		if (strcmp((char*)pkt->data, FLUSH_DATA) == 0)
		{
			avcodec_flush_buffers(mAudioStream->codec);
			av_packet_unref(pkt);
			continue;
		}

		if (seek_flag_audio)
		{
			//发生了跳转 则跳过关键帧到目的时间的这几帧
			if (audio_clock < seek_time)
			{
				continue;
			}
			else
			{
				seek_flag_audio = 0;
			}
		}

		//解码AVPacket->AVFrame
		int got_frame = 0;
		int size = avcodec_decode_audio4(aCodecCtx, aFrame, &got_frame, &packet);

		//保存重采样之前的一个声道的数据方法
		//size_t unpadded_linesize = aFrame->nb_samples * av_get_bytes_per_sample((AVSampleFormat) aFrame->format);
		//static FILE * fp = fopen("out.pcm", "wb");
		//fwrite(aFrame->extended_data[0], 1, unpadded_linesize, fp);

		av_packet_unref(&packet);

		if (got_frame)
		{
			/// ffmpeg解码之后得到的音频数据不是SDL想要的，
			/// 因此这里需要重采样成44100 双声道 AV_SAMPLE_FMT_S16

			///
			/// 需要保证重采样后音频的时间是相同的，不同采样率下同样时间采集的数据采样点个数肯定不一样。
			/// 因此就需要重新计算采样点个数（使用下面的函数）
			/// 将in_sample_rate的采样次数换算成out_sample_rate对应的采样次数
            int nb_samples = av_rescale_rnd(swr_get_delay(swrCtx, out_sample_rate) + aFrame->nb_samples, out_sample_rate, in_sample_rate, AV_ROUND_UP);

            if (aFrame_ReSample != nullptr)
            {
                if (aFrame_ReSample->nb_samples != nb_samples)
                {
                    av_frame_free(&aFrame_ReSample);
                    aFrame_ReSample = nullptr;
                }
            }

            ///解码一帧后才能获取到采样率等信息，因此将初始化放到这里
            if (aFrame_ReSample == nullptr)
            {
                aFrame_ReSample = av_frame_alloc();

                aFrame_ReSample->format = out_sample_fmt;
                aFrame_ReSample->channel_layout = out_ch_layout;
                aFrame_ReSample->sample_rate = out_sample_rate;
                aFrame_ReSample->nb_samples = nb_samples;

                int ret = av_samples_fill_arrays(aFrame_ReSample->data, aFrame_ReSample->linesize, audio_buf, audio_tgt_channels, aFrame_ReSample->nb_samples, out_sample_fmt, 0);
                //                int ret = av_frame_get_buffer(aFrame_ReSample, 0);
                if (ret < 0)
                {
                    fprintf(stderr, "Error allocating an audio buffer\n");
                    //                        exit(1);
                }
            }

            int len2 = swr_convert(swrCtx, aFrame_ReSample->data, aFrame_ReSample->nb_samples, (const uint8_t**)aFrame->data, aFrame->nb_samples);

            ///下面这两种方法计算的大小是一样的
#if 0
            int resampled_data_size = len2 * audio_tgt_channels * av_get_bytes_per_sample(out_sample_fmt);
#else
            int resampled_data_size = av_samples_get_buffer_size(NULL, audio_tgt_channels, aFrame_ReSample->nb_samples, out_sample_fmt, 1);
#endif
            //qDebug()<<__FUNCTION__<<resampled_data_size<<aFrame_ReSample->nb_samples<<aFrame->nb_samples;
            audioBufferSize = resampled_data_size;
            break;

		}
	}

	return audioBufferSize;
}

void VideoPlayer::decodeAudioThread()
{
	mIsAudioThreadFinished = false;

	while (!mIsQuit) {
		if (mIsPause) {
			QThread::msleep(5);  // 短暂休眠避免占用 CPU
			continue;
		}

		// 解码音频帧
		int audio_data_size = decodeAudioFrame1();

		// 如果解码出错或没有数据，播放静音
		if (audio_data_size <= 0) {
			memset(audio_buf, 0, 1024);
			audio_data_size = 1024;
		}

		// 如果需要静音处理
		if (mIsMute || mIsNeedPause) {
			memset(audio_buf, 0, audio_data_size);
		}
		else {
			PcmVolumeControl::RaiseVolume((char*)audio_buf, audio_data_size, 1, mVolume); // 调节音量
		}
        if (audioDevice) {
            qint64 bytesWritten = 0;
            while (bytesWritten < audio_data_size) {
                qint64 written = audioDevice->write((const char*)audio_buf + bytesWritten, audio_data_size - bytesWritten);

                if (written == -1) {
                    qDebug() << "Error writing audio data";
                    break;
                }
                else if (written == 0) {
                    //QThread::msleep(1);  // 避免过度等待，降低 msleep
                    av_usleep(50);
                }
                else {

                    bytesWritten += written;
                }
            }

        }


		// 短暂休眠，防止占用过多 CPU
		QThread::msleep(5);
	}

	mIsAudioThreadFinished = true;
}

int VideoPlayer::decodeAudioFrame1(bool isBlock)
{
	int audioBufferSize = 0;

	while (1)
	{
		if (mIsQuit) {
			mIsAudioThreadFinished = true;
			clearAudioQueue();
			break;
		}

		if (mIsPause) {
			break;
		}

		mConditon_Audio->Lock();

		if (mAudioPacktList.size() <= 0) {
			if (isBlock) {
				mConditon_Audio->Wait();
			}
			else {
				mConditon_Audio->Unlock();
				break;
			}
		}

		AVPacket packet = mAudioPacktList.front();
		mAudioPacktList.pop_front();
		mConditon_Audio->Unlock();

		AVPacket* pkt = &packet;

		if (pkt->pts != AV_NOPTS_VALUE)
		{
			audio_clock = av_q2d(mAudioStream->time_base) * pkt->pts;
		}
        //audio_clock /= mSpeed;
		//收到这个数据 说明刚刚执行过跳转 现在需要把解码器的数据 清除一下
		if (strcmp((char*)pkt->data, FLUSH_DATA) == 0)
		{
			avcodec_flush_buffers(mAudioStream->codec);
			av_packet_unref(pkt);
			continue;
		}

		if (seek_flag_audio)
		{
			//发生了跳转 则跳过关键帧到目的时间的这几帧
			if (audio_clock < seek_time)
			{
				av_packet_unref(pkt);
				continue;
			}
			else
			{
				seek_flag_audio = 0;
			}
		}

		// 发送数据包到解码器
		if (avcodec_send_packet(aCodecCtx, &packet) != 0) {
			av_packet_unref(&packet);
			continue;
		}

		// 接收解码后的帧
		int ret = avcodec_receive_frame(aCodecCtx, aFrame);

		av_packet_unref(&packet);

		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			continue; // 数据不足，继续解码
		}
		else if (ret < 0) {
			break; // 解码错误
		}

		// 处理解码后的帧
		// 重采样音频数据为 QAudioOutput 支持的格式
		int nb_samples = av_rescale_rnd(swr_get_delay(swrCtx, out_sample_rate) + aFrame->nb_samples, 
			out_sample_rate, in_sample_rate, AV_ROUND_UP);


		// 打印重采样后的采样点数
        //qDebug() << "nb_samples after resampling:" << nb_samples;

		// 初始化重采样帧
		if (!aFrame_ReSample || aFrame_ReSample->nb_samples != nb_samples) {
			if (aFrame_ReSample) {
				av_frame_free(&aFrame_ReSample);
			}
			aFrame_ReSample = av_frame_alloc();

			aFrame_ReSample->format = out_sample_fmt;
			aFrame_ReSample->channel_layout = out_ch_layout;
			aFrame_ReSample->sample_rate = out_sample_rate;
			aFrame_ReSample->nb_samples = nb_samples;

			int ret = av_samples_fill_arrays(aFrame_ReSample->data, aFrame_ReSample->linesize, 
				audio_buf, audio_tgt_channels, 
				aFrame_ReSample->nb_samples, out_sample_fmt, 0);
			if (ret < 0) {
				fprintf(stderr, "Error allocating an audio buffer\n");
				continue;
			}
		}

		// 使用 swr_convert 进行重采样
        int len = swr_convert(swrCtx, aFrame_ReSample->data, aFrame_ReSample->nb_samples, (const uint8_t**)aFrame->data, aFrame->nb_samples);

        audioBufferSize = av_samples_get_buffer_size(nullptr, audio_tgt_channels, len, out_sample_fmt, 1);

		break; // 成功解码一帧音频，退出循环
	}
	
	return audioBufferSize;
}

