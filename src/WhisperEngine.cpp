#include "WhisperEngine.h"

#include <whisper.h>
#include <QFileInfo>
#include <algorithm>
#include <cstring>
#include <thread>
#include <vector>

QString WhisperEngine::transcribe(const QByteArray &audio, const QString &modelPath,
                                  const QString &language, QString *error)
{
    if (!QFileInfo(modelPath).isFile()) {
        *error = QStringLiteral("Select a valid Whisper model file.");
        return {};
    }
    if (audio.size() < int(sizeof(float) * 1600)) {
        *error = QStringLiteral("The recording was too short.");
        return {};
    }

    whisper_context_params contextParams = whisper_context_default_params();
    whisper_context *context = whisper_init_from_file_with_params(modelPath.toUtf8().constData(), contextParams);
    if (!context) {
        *error = QStringLiteral("Could not load the Whisper model.");
        return {};
    }

    std::vector<float> samples(size_t(audio.size() / int(sizeof(float))));
    std::memcpy(samples.data(), audio.constData(), size_t(audio.size()));
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.no_timestamps = true;
    params.single_segment = false;
    params.n_threads = std::max(1U, std::thread::hardware_concurrency() / 2);
    const QByteArray languageUtf8 = language.toUtf8();
    params.language = languageUtf8.constData();

    const int result = whisper_full(context, params, samples.data(), int(samples.size()));
    if (result != 0) {
        *error = QStringLiteral("Whisper could not transcribe the recording.");
        whisper_free(context);
        return {};
    }

    QString text;
    const int segments = whisper_full_n_segments(context);
    for (int i = 0; i < segments; ++i)
        text += QString::fromUtf8(whisper_full_get_segment_text(context, i));
    whisper_free(context);
    return text.trimmed();
}
