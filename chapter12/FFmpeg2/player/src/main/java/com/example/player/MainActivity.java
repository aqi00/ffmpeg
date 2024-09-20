package com.example.player;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        findViewById(R.id.btn_simple).setOnClickListener(this);
        findViewById(R.id.btn_ad).setOnClickListener(this);
        findViewById(R.id.btn_list).setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        if (v.getId() == R.id.btn_simple) {
            Intent intent = new Intent(this, SimplePlayerActivity.class);
            startActivity(intent);
        } else if (v.getId() == R.id.btn_ad) {
            Intent intent = new Intent(this, AdPlayerActivity.class);
            startActivity(intent);
        } else if (v.getId() == R.id.btn_list) {
            Intent intent = new Intent(this, ListPlayerActivity.class);
            startActivity(intent);
        }
    }
}