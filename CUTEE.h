#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_CUTEE.h"

class CUTEE : public QMainWindow
{
    Q_OBJECT

public:
    CUTEE(QWidget *parent = nullptr);
    ~CUTEE();

private:
    Ui::CUTEEClass ui;
};

