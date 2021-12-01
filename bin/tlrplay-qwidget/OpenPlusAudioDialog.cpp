// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021 Darby Johnston
// All rights reserved.

#include "OpenPlusAudioDialog.h"

#include <tlrCore/String.h>
#include <tlrCore/Timeline.h>

#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QPushButton>

namespace tlr
{
    OpenPlusAudioDialog::OpenPlusAudioDialog(
        const std::shared_ptr<core::Context>& context,
        QWidget* parent) :
        QDialog(parent),
        _context(context)
    {
        setWindowTitle(tr("Open + Audio"));

        auto mediaGroupBox = new QGroupBox(tr("Media"));
        _mediaLineEdit = new QLineEdit;
        auto mediaBrowseButton = new QPushButton(tr("Browse"));

        auto audioGroupBox = new QGroupBox(tr("Audio"));
        _audioLineEdit = new QLineEdit;
        auto audioBrowseButton = new QPushButton(tr("Browse"));

        auto buttonBox = new QDialogButtonBox;
        buttonBox->addButton(QDialogButtonBox::Ok);
        buttonBox->addButton(QDialogButtonBox::Cancel);

        auto layout = new QVBoxLayout;
        auto vLayout = new QVBoxLayout;
        auto hLayout = new QHBoxLayout;
        hLayout->addWidget(_mediaLineEdit);
        hLayout->addWidget(mediaBrowseButton);
        mediaGroupBox->setLayout(hLayout);
        vLayout->addWidget(mediaGroupBox);
        hLayout = new QHBoxLayout;
        hLayout->addWidget(_audioLineEdit);
        hLayout->addWidget(audioBrowseButton);
        audioGroupBox->setLayout(hLayout);
        vLayout->addWidget(audioGroupBox);
        layout->addLayout(vLayout);
        layout->addWidget(buttonBox);
        setLayout(layout);

        connect(
            _mediaLineEdit,
            SIGNAL(textChanged(const QString&)),
            SLOT(_mediaLineEditCallback(const QString&)));

        connect(
            mediaBrowseButton,
            SIGNAL(clicked()),
            SLOT(_browseMediaCallback()));

        connect(
            _audioLineEdit,
            SIGNAL(textChanged(const QString&)),
            SLOT(_audioLineEditCallback(const QString&)));

        connect(
            audioBrowseButton,
            SIGNAL(clicked()),
            SLOT(_browseAudioCallback()));

        connect(
            buttonBox,
            SIGNAL(accepted()),
            SLOT(accept()));
        connect(
            buttonBox,
            SIGNAL(rejected()),
            SLOT(reject()));
    }

    const QString& OpenPlusAudioDialog::mediaFileName() const
    {
        return _mediaFileName;
    }

    const QString& OpenPlusAudioDialog::audioFileName() const
    {
        return _audioFileName;
    }

    void OpenPlusAudioDialog::_mediaLineEditCallback(const QString& value)
    {
        _mediaFileName = value.toUtf8().data();
    }

    void OpenPlusAudioDialog::_browseMediaCallback()
    {
        if (auto context = _context.lock())
        {
            std::vector<std::string> extensions;
            for (const auto& i : timeline::getExtensions(
                static_cast<int>(avio::FileExtensionType::VideoAndAudio) |
                static_cast<int>(avio::FileExtensionType::VideoOnly),
                context))
            {
                extensions.push_back("*" + i);
            }

            const auto fileName = QFileDialog::getOpenFileName(
                this,
                tr("Open Media"),
                _mediaFileName,
                tr("Files") + " (" + QString::fromUtf8(string::join(extensions, " ").c_str()) + ")");
            if (!fileName.isEmpty())
            {
                _mediaFileName = fileName;
                _mediaLineEdit->setText(_mediaFileName);
            }
        }
    }

    void OpenPlusAudioDialog::_audioLineEditCallback(const QString& value)
    {
        _audioFileName = value.toUtf8().data();
    }

    void OpenPlusAudioDialog::_browseAudioCallback()
    {
        if (auto context = _context.lock())
        {
            std::vector<std::string> extensions;
            for (const auto& i : timeline::getExtensions(
                static_cast<int>(avio::FileExtensionType::AudioOnly),
                context))
            {
                extensions.push_back("*" + i);
            }

            const auto fileName = QFileDialog::getOpenFileName(
                this,
                tr("Open Audio"),
                _audioFileName,
                tr("Files") + " (" + QString::fromUtf8(string::join(extensions, " ").c_str()) + ")");
            if (!fileName.isEmpty())
            {
                _audioFileName = fileName;
                _audioLineEdit->setText(_audioFileName);
            }
        }
    }
}
